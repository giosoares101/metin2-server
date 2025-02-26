#include "stdafx.h"
#include "config.h"
#include "utils.h"
#include "desc.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char.h"
#include "protocol.h"
#include "packet.h"
#include "messenger_manager.h"
#include "sectree_manager.h"
#include "p2p.h"
#include "buffer_manager.h"
#include "sequence.h"
#include "guild.h"
#include "guild_manager.h"
#include "TrafficProfiler.h"
#include "locale_service.h"
#include "log.h"

extern int max_bytes_written;
extern int current_bytes_written;
extern int total_bytes_written;

void DescReadHandler(bufferevent *bev, void *ctx) {
    auto* d = (LPDESC) ctx;

    if (db_clientdesc == d)
    {
        int size = d->ProcessInput();

        if (size < 0)
        {
            d->SetPhase(PHASE_CLOSE);
        }
    }
    else if (!d->ProcessInput())
    {
        d->SetPhase(PHASE_CLOSE);
    }
}

void DescWriteHandler(bufferevent *bev, void *ctx) {
    auto* d = (LPDESC) ctx;

    if (db_clientdesc == d)
    {
        evbuffer *output = bufferevent_get_output(bev);
        size_t buf_size = evbuffer_get_length(output);
    }
}

void DescEventHandler(bufferevent *bev, short events, void *ctx) {
    auto* d = (LPDESC) ctx;

    if (events & BEV_EVENT_ERROR)
        SPDLOG_ERROR("DESC libevent error, handle: {}", d->GetHandle());

    if (events & BEV_EVENT_EOF)
        SPDLOG_DEBUG("DESC disconnected: handle {}", d->GetHandle());

    // Either the socket was closed or an error occured, therefore we can disconnect this peer.
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR))
        d->SetPhase(PHASE_CLOSE);
}

DESC::DESC()
{
	Initialize();
}

DESC::~DESC()
{
}

void DESC::Initialize()
{
	m_bDestroyed = false;

	m_pInputProcessor = NULL;
	m_evbase = nullptr;
	m_bufevent = nullptr;
	m_iPhase = PHASE_CLOSE;
	m_dwHandle = 0;

	m_wPort = 0;
	m_LastTryToConnectTime = 0;

	m_dwHandshake = 0;
	m_dwHandshakeSentTime = 0;
	m_iHandshakeRetry = 0;
	m_dwClientTime = 0;
	m_bHandshaking = false;

	m_pkPingEvent = NULL;
	m_lpCharacter = NULL;
	memset( &m_accountTable, 0, sizeof(m_accountTable) );

	m_wP2PPort = 0;
	m_bP2PChannel = 0;

	m_bAdminMode = false;
	m_bPong = true;
	m_bChannelStatusRequested = false;

	m_iCurrentSequence = 0;

	m_dwMatrixRows = m_dwMatrixCols = 0;
	m_bMatrixTryCount = 0;

	m_pkLoginKey = NULL;
	m_dwLoginKey = 0;
	m_dwPanamaKey = 0;

	m_bCRCMagicCubeIdx = 0;
	m_dwProcCRC = 0;
	m_dwFileCRC = 0;

	m_outtime = 0;
	m_playtime = 0;
	m_offtime = 0;

	m_pkDisconnectEvent = NULL;

	m_seq_vector.clear();
}

void DESC::Destroy()
{
	if (m_bDestroyed) {
		return;
	}
	m_bDestroyed = true;

	if (m_pkLoginKey)
		m_pkLoginKey->Expire();

	if (GetAccountTable().id)
		DESC_MANAGER::instance().DisconnectAccount(GetAccountTable().login);

	if (m_lpCharacter)
	{
		m_lpCharacter->Disconnect("DESC::~DESC");
		m_lpCharacter = NULL;
	}

	event_cancel(&m_pkPingEvent);
	event_cancel(&m_pkDisconnectEvent);

	if (!g_bAuthServer)
	{
		if (m_accountTable.login[0] && m_accountTable.passwd[0])
		{
			TLogoutPacket pack;

			strlcpy(pack.login, m_accountTable.login, sizeof(pack.login));
			strlcpy(pack.passwd, m_accountTable.passwd, sizeof(pack.passwd));

			db_clientdesc->DBPacket(HEADER_GD_LOGOUT, m_dwHandle, &pack, sizeof(TLogoutPacket));
		}
	}

	if (m_bufevent != nullptr)
	{
		SPDLOG_DEBUG("SYSTEM: closing socket.");

        bufferevent_free(m_bufevent);
        m_bufevent = nullptr;
	}

	m_seq_vector.clear();
}

EVENTFUNC(ping_event)
{
	DESC::desc_event_info* info = dynamic_cast<DESC::desc_event_info*>( event->info );

	if ( info == NULL )
	{
		SPDLOG_ERROR("ping_event> <Factor> Null pointer" );
		return 0;
	}

	LPDESC desc = info->desc;

	if (desc->IsAdminMode())
		return (ping_event_second_cycle);

	if (!desc->IsPong())
	{
		SPDLOG_WARN("PING_EVENT: no pong {}", desc->GetHostName());

		desc->SetPhase(PHASE_CLOSE);

		return (ping_event_second_cycle);
	}
	else
	{
		TPacketGCPing p;
		p.header = HEADER_GC_PING;
		desc->Packet(&p, sizeof(struct packet_ping));
		desc->SetPong(false);
	}

	desc->SendHandshake(get_dword_time(), 0);

	return (ping_event_second_cycle);
}

bool DESC::IsPong()
{
	return m_bPong;
}

void DESC::SetPong(bool b)
{
	m_bPong = b;
}

bool DESC::Setup(event_base * evbase, evutil_socket_t fd, const sockaddr * c_rSockAddr, DWORD _handle, DWORD _handshake)
{
    m_bufevent = bufferevent_socket_new(evbase, fd, BEV_OPT_CLOSE_ON_FREE);
    if (m_bufevent == nullptr) {
        SPDLOG_ERROR("DESC::Setup : Could not set up bufferevent!");
        return false;
    }

    // Set the event handlers for this peer
    bufferevent_setcb(m_bufevent, DescReadHandler, DescWriteHandler, DescEventHandler, this);

    // Enable the events
    bufferevent_enable(m_bufevent, EV_READ|EV_WRITE);

	m_stHost		= GetSocketHost(c_rSockAddr);
	m_wPort			= GetSocketPort(c_rSockAddr);
	m_dwHandle		= _handle;

	// Ping Event
	desc_event_info* info = AllocEventInfo<desc_event_info>();

	info->desc = this;
	assert(m_pkPingEvent == NULL);

	m_pkPingEvent = event_create(ping_event, info, ping_event_second_cycle);

	// Set Phase to handshake
	SetPhase(PHASE_HANDSHAKE);
	StartHandshake(_handshake);

	SPDLOG_DEBUG("SYSTEM: new connection from [{}] handshake {}, ptr {}", m_stHost, m_dwHandshake, (void*) this);

	return true;
}

bool DESC::ProcessInput()
{
    evbuffer *input = bufferevent_get_input(m_bufevent);
    if (input == nullptr) {
        SPDLOG_ERROR("DESC::ProcessInput : nil input buffer");
        return false;
    }

	if (!m_pInputProcessor) {
        SPDLOG_ERROR("no input processor");
        return false;
    }

    // If CInputProcessor::Process returns false, then we switched to another phase, and we can continue reading
    bool doContinue = true;

    do {
        size_t bytes_read = evbuffer_get_length(input);

        // No data to read, we can continue
        if (bytes_read == 0)
            return true;

        // Get the received data
        void * data = evbuffer_pullup(input, bytes_read);

        int iBytesProceed = 0; // The number of bytes that have been processed
        doContinue = m_pInputProcessor->Process(this, data, bytes_read, iBytesProceed);

        // Flush the read bytes from the network buffer
        evbuffer_drain(input, iBytesProceed);
        iBytesProceed = 0;
    } while(!doContinue);

    return true;
}

bool DESC::RawPacket(const void * c_pvData, int iSize)
{
	if (m_iPhase == PHASE_CLOSE)
		return false;

    if (!m_bufevent) {
        SPDLOG_ERROR("Bufferevent not ready!");
        return false;
    }

    if (bufferevent_write(m_bufevent, c_pvData, iSize) != 0) {
        SPDLOG_ERROR("Buffer write error!");
        return false;
    }

    return true;
}

void DESC::Packet(const void * c_pvData, int iSize)
{
	assert(iSize > 0);

	if (m_iPhase == PHASE_CLOSE) // 끊는 상태면 보내지 않는다.
		return;

	if (m_stRelayName.length() != 0)
	{
		// Relay 패킷은 암호화하지 않는다.
		TPacketGGRelay p;

		p.bHeader = HEADER_GG_RELAY;
		strlcpy(p.szName, m_stRelayName.c_str(), sizeof(p.szName));
		p.lSize = iSize;

		if (!RawPacket(&p, sizeof(p)))
		{
			m_iPhase = PHASE_CLOSE;
			return;
		}

		m_stRelayName.clear();

		if (!RawPacket(c_pvData, iSize))
		{
			m_iPhase = PHASE_CLOSE;
			return;
		}
	}
	else
	{
		// TRAFFIC_PROFILE
		if (g_bTrafficProfileOn)
			TrafficProfiler::instance().Report(TrafficProfiler::IODIR_OUTPUT, *(BYTE *) c_pvData, iSize);
		// END_OF_TRAFFIC_PROFILER

		if (!RawPacket(c_pvData, iSize)) {
			m_iPhase = PHASE_CLOSE;
		}
    }
}

void DESC::SetPhase(int _phase)
{
	m_iPhase = _phase;

	TPacketGCPhase pack;
	pack.header = HEADER_GC_PHASE;
	pack.phase = _phase;
	Packet(&pack, sizeof(TPacketGCPhase));

	switch (m_iPhase)
	{
		case PHASE_CLOSE:
			// 메신저가 캐릭터단위가 되면서 삭제
			//MessengerManager::instance().Logout(GetAccountTable().login);
			m_pInputProcessor = &m_inputClose;
			break;

		case PHASE_HANDSHAKE:
			m_pInputProcessor = &m_inputHandshake;
			break;

		case PHASE_SELECT:
			// 메신저가 캐릭터단위가 되면서 삭제
			//MessengerManager::instance().Logout(GetAccountTable().login); // 의도적으로 break 안검
		case PHASE_LOGIN:
		case PHASE_LOADING:
			m_pInputProcessor = &m_inputLogin;
			break;

		case PHASE_GAME:
		case PHASE_DEAD:
			m_pInputProcessor = &m_inputMain;
			break;

		case PHASE_AUTH:
			m_pInputProcessor = &m_inputAuth;
			SPDLOG_DEBUG("AUTH_PHASE {}", (void*) this);
			break;
	}
}

void DESC::BindAccountTable(TAccountTable * pAccountTable)
{
	assert(pAccountTable != NULL);
	memcpy(&m_accountTable, pAccountTable, sizeof(TAccountTable));
	DESC_MANAGER::instance().ConnectAccount(m_accountTable.login, this);
}

void DESC::StartHandshake(DWORD _handshake)
{
	// Handshake
	m_dwHandshake = _handshake;

	SendHandshake(get_dword_time(), 0);

	m_iHandshakeRetry = 0;
}

void DESC::SendHandshake(DWORD dwCurTime, LONG lNewDelta)
{
	TPacketGCHandshake pack;

	pack.bHeader		= HEADER_GC_HANDSHAKE;
	pack.dwHandshake	= m_dwHandshake;
	pack.dwTime			= dwCurTime;
	pack.lDelta			= lNewDelta;

	Packet(&pack, sizeof(TPacketGCHandshake));

	m_dwHandshakeSentTime = dwCurTime;
	m_bHandshaking = true;
}

bool DESC::HandshakeProcess(DWORD dwTime, LONG lDelta, bool bInfiniteRetry)
{
	DWORD dwCurTime = get_dword_time();

	if (lDelta < 0)
	{
		SPDLOG_ERROR("Desc::HandshakeProcess : value error (lDelta {}, ip {})", lDelta, m_stHost.c_str());
		return false;
	}

    int bias = (int) (dwCurTime - (dwTime + lDelta));

	if (bias >= 0 && bias <= 50)
	{
		if (bInfiniteRetry)
		{
			BYTE bHeader = HEADER_GC_TIME_SYNC;
			Packet(&bHeader, sizeof(BYTE));
		}

		if (GetCharacter())
			SPDLOG_DEBUG("Handshake: client_time {} server_time {} name: {}", m_dwClientTime, dwCurTime, GetCharacter()->GetName());
		else
			SPDLOG_DEBUG("Handshake: client_time {} server_time {}", m_dwClientTime, dwCurTime, lDelta);

		m_dwClientTime = dwCurTime;
		m_bHandshaking = false;
		return true; 
	}

	LONG lNewDelta = (LONG) (dwCurTime - dwTime) / 2;

	if (lNewDelta < 0)
	{
		SPDLOG_DEBUG("Handshake: lower than zero {}", lNewDelta);
		lNewDelta = (dwCurTime - m_dwHandshakeSentTime) / 2;
	}

	SPDLOG_DEBUG("Handshake: ServerTime {} dwTime {} lDelta {} SentTime {} lNewDelta {}", dwCurTime, dwTime, lDelta, m_dwHandshakeSentTime, lNewDelta);

	if (!bInfiniteRetry)
		if (++m_iHandshakeRetry > HANDSHAKE_RETRY_LIMIT)
		{
			SPDLOG_ERROR("handshake retry limit reached! (limit {} character {})",
					HANDSHAKE_RETRY_LIMIT, GetCharacter() ? GetCharacter()->GetName() : "!NO CHARACTER!");
			SetPhase(PHASE_CLOSE);
			return false;
		}

	SendHandshake(dwCurTime, lNewDelta);
	return false;
}

bool DESC::IsHandshaking()
{
	return m_bHandshaking;
}

DWORD DESC::GetClientTime()
{
	return m_dwClientTime;
}

void DESC::SetRelay(const char * c_pszName)
{
	m_stRelayName = c_pszName;
}

void DESC::BindCharacter(LPCHARACTER ch)
{
	m_lpCharacter = ch;
}

void DESC::FlushOutput()
{
	if (m_bufevent == nullptr)
		return;

    if (bufferevent_flush(m_bufevent, EV_WRITE, BEV_FLUSH) < 0)
        SPDLOG_ERROR("FLUSH FAIL");

    // TODO: investigate if necessary
	usleep(250000);
}

EVENTFUNC(disconnect_event)
{
	DESC::desc_event_info* info = dynamic_cast<DESC::desc_event_info*>( event->info );

	if ( info == NULL )
	{
		SPDLOG_ERROR("disconnect_event> <Factor> Null pointer" );
		return 0;
	}

	LPDESC d = info->desc;

	d->m_pkDisconnectEvent = NULL;
	d->SetPhase(PHASE_CLOSE);
	return 0;
}

bool DESC::DelayedDisconnect(int iSec)
{
	if (m_pkDisconnectEvent != NULL) {
		return false;
	}

	desc_event_info* info = AllocEventInfo<desc_event_info>();
	info->desc = this;

	m_pkDisconnectEvent = event_create(disconnect_event, info, PASSES_PER_SEC(iSec));
	return true;
}

void DESC::DisconnectOfSameLogin()
{
	if (GetCharacter())
	{
		if (m_pkDisconnectEvent)
			return;

		GetCharacter()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Someone has logged into your account. You will be disconnected from the server."));
		DelayedDisconnect(5);
	}
	else
	{
		SetPhase(PHASE_CLOSE);
	}
}

void DESC::SetAdminMode()
{
	m_bAdminMode = true;
}

bool DESC::IsAdminMode()
{
	return m_bAdminMode;
}

BYTE DESC::GetSequence()
{
	return gc_abSequence[m_iCurrentSequence];
}

void DESC::SetNextSequence()
{
	if (++m_iCurrentSequence == SEQUENCE_MAX_NUM)
		m_iCurrentSequence = 0;
}

void DESC::SendLoginSuccessPacket()
{
	TAccountTable & rTable = GetAccountTable();

	TPacketGCLoginSuccess p;

	p.bHeader    = HEADER_GC_LOGIN_SUCCESS_NEWSLOT;

	p.handle     = GetHandle();
	p.random_key = DESC_MANAGER::instance().MakeRandomKey(GetHandle()); // FOR MARK
	memcpy(p.players, rTable.players, sizeof(rTable.players));

	for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i)
	{   
		CGuild* g = CGuildManager::instance().GetLinkedGuild(rTable.players[i].dwID);

		if (g)
		{   
			p.guild_id[i] = g->GetID();
			strlcpy(p.guild_name[i], g->GetName(), sizeof(p.guild_name[i]));
		}   
		else
		{
			p.guild_id[i] = 0;
			p.guild_name[i][0] = '\0';
		}
	}

	Packet(&p, sizeof(TPacketGCLoginSuccess));
}

//void DESC::SendServerStatePacket(int nIndex)
//{
//	TPacketGCStateCheck rp;
//
//	int iTotal; 
//	int * paiEmpireUserCount;
//	int iLocal;
//
//	DESC_MANAGER::instance().GetUserCount(iTotal, &paiEmpireUserCount, iLocal);
//
//	rp.header	= 1; 
//	rp.key		= 0;
//	rp.index	= nIndex;
//
//	if (g_bNoMoreClient) rp.state = 0;
//	else rp.state = iTotal > g_iFullUserCount ? 3 : iTotal > g_iBusyUserCount ? 2 : 1;
//	
//	this->Packet(&rp, sizeof(rp));
//	//printf("STATE_CHECK PACKET PROCESSED.\n");
//}

void DESC::SetMatrixCardRowsAndColumns(unsigned int rows, unsigned int cols)
{
	m_dwMatrixRows = rows;
	m_dwMatrixCols = cols;
}

unsigned int DESC::GetMatrixRows()
{
	return m_dwMatrixRows;
}

unsigned int DESC::GetMatrixCols()
{
	return m_dwMatrixCols;
}

bool DESC::CheckMatrixTryCount()
{
	if (++m_bMatrixTryCount >= 3)
		return false;

	return true;
}

void DESC::SetLoginKey(DWORD dwKey)
{
	m_dwLoginKey = dwKey;
}

void DESC::SetLoginKey(CLoginKey * pkKey)
{
	m_pkLoginKey = pkKey;
	SPDLOG_DEBUG("SetLoginKey {}", m_pkLoginKey->m_dwKey);
}

DWORD DESC::GetLoginKey()
{
	if (m_pkLoginKey)
		return m_pkLoginKey->m_dwKey;

	return m_dwLoginKey;
}

const BYTE* GetKey_20050304Myevan()
{   
	static bool bGenerated = false;
	static DWORD s_adwKey[1938]; 

	if (!bGenerated) 
	{
		bGenerated = true;
		DWORD seed = 1491971513; 

		for (UINT i = 0; i < BYTE(seed); ++i)
		{
			seed ^= 2148941891ul;
			seed += 3592385981ul;

			s_adwKey[i] = seed;
		}
	}

	return (const BYTE*)s_adwKey;
}

void DESC::AssembleCRCMagicCube(BYTE bProcPiece, BYTE bFilePiece)
{
	static BYTE abXORTable[32] =
	{
		102,  30, 0, 0, 0, 0, 0, 0,
		188,  44, 0, 0, 0, 0, 0, 0,
		39, 201, 0, 0, 0, 0, 0, 0,
		43,   5, 0, 0, 0, 0, 0, 0,
	};

	bProcPiece = (bProcPiece ^ abXORTable[m_bCRCMagicCubeIdx]);
	bFilePiece = (bFilePiece ^ abXORTable[m_bCRCMagicCubeIdx+1]);

	m_dwProcCRC |= bProcPiece << m_bCRCMagicCubeIdx;
	m_dwFileCRC |= bFilePiece << m_bCRCMagicCubeIdx;

	m_bCRCMagicCubeIdx += 8;

	if (!(m_bCRCMagicCubeIdx & 31))
	{
		m_dwProcCRC = 0;
		m_dwFileCRC = 0;
		m_bCRCMagicCubeIdx = 0;
	}
}

void DESC::push_seq(BYTE hdr, BYTE seq)
{
	if (m_seq_vector.size()>=20)
	{
		m_seq_vector.erase(m_seq_vector.begin());
	}

	seq_t info = { hdr, seq };
	m_seq_vector.push_back(info);
}

BYTE DESC::GetEmpire()
{
	return m_accountTable.bEmpire;
}

void DESC::ChatPacket(BYTE type, const char * format, ...)
{
	char chatbuf[CHAT_MAX_LEN + 1];
	va_list args;

	va_start(args, format);
	int len = vsnprintf(chatbuf, sizeof(chatbuf), format, args);
	va_end(args);

	struct packet_chat pack_chat;

	pack_chat.header    = HEADER_GC_CHAT;
	pack_chat.size      = sizeof(struct packet_chat) + len;
	pack_chat.type      = type;
	pack_chat.id        = 0;
	pack_chat.bEmpire   = GetEmpire();

	TEMP_BUFFER buf;
	buf.write(&pack_chat, sizeof(struct packet_chat));
	buf.write(chatbuf, len);

	Packet(buf.read_peek(), buf.size());
}

