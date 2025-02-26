#include "stdafx.h"
#include "config.h"
#include "utils.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char.h"
#include "protocol.h"
#include "p2p.h"
#include "buffer_manager.h"
#include "guild_manager.h"
#include "db.h"
#include "party.h"

LPCLIENT_DESC db_clientdesc = NULL;
LPCLIENT_DESC g_pkAuthMasterDesc = NULL;
LPCLIENT_DESC g_NetmarbleDBDesc = NULL;

static const char* GetKnownClientDescName(LPCLIENT_DESC desc) {
	if (desc == db_clientdesc) {
		return "db_clientdesc";
	} else if (desc == g_pkAuthMasterDesc) {
		return "g_pkAuthMasterDesc";
	} else if (desc == g_NetmarbleDBDesc) {
		return "g_NetmarbleDBDesc";
	} 
	return "unknown";
}

void ClientDescEventHandler(bufferevent *bev, short events, void *ptr) {
    auto * clientDesc = static_cast<LPCLIENT_DESC>(ptr);

    if (events & BEV_EVENT_CONNECTED) {
        SPDLOG_DEBUG("SYSTEM: connected to server (ptr {})", (void*) clientDesc);
        clientDesc->OnConnectSuccessful();

        // Now that we're connected, we can set the read/write/event handlers (and therefore stop using this handler)
        auto * desc = static_cast<LPDESC>(clientDesc);
        bufferevent_setcb(bev, DescReadHandler, DescWriteHandler, DescEventHandler, desc);
    } else if (events & (BEV_EVENT_ERROR|BEV_EVENT_EOF)) {
        if (events & BEV_EVENT_ERROR) {
            int err = bufferevent_socket_get_dns_error(bev);
            if (err)
                SPDLOG_ERROR("SYSTEM: Client connection DNS error: {}", evutil_gai_strerror(err));
        }

        SPDLOG_DEBUG("SYSTEM: closing client connection (ptr {})", (void*) clientDesc);
        clientDesc->SetPhase(PHASE_CLOSE);
    }
}

CLIENT_DESC::CLIENT_DESC()
{
	m_iPhaseWhenSucceed = 0;
	m_bRetryWhenClosed = false;
	m_LastTryToConnectTime = 0;
	m_tLastChannelStatusUpdateTime = 0;
}

CLIENT_DESC::~CLIENT_DESC()
{
}

void CLIENT_DESC::Destroy()
{
	if (m_bufevent == nullptr)
		return;

	P2P_MANAGER::instance().UnregisterConnector(this);

	if (this == db_clientdesc)
	{
		CPartyManager::instance().DeleteAllParty();
		CPartyManager::instance().DisablePCParty();
		CGuildManager::instance().StopAllGuildWar();
	}

	SPDLOG_DEBUG("SYSTEM: closing client socket.");

	bufferevent_free(m_bufevent);
    m_bufevent = nullptr;

	// Chain up to base class Destroy()
	DESC::Destroy();
}

void CLIENT_DESC::SetRetryWhenClosed(bool b)
{
	m_bRetryWhenClosed = b;
}

bool CLIENT_DESC::Connect(int iPhaseWhenSucceed)
{
	if (iPhaseWhenSucceed != 0)
		m_iPhaseWhenSucceed = iPhaseWhenSucceed;

	if (get_global_time() - m_LastTryToConnectTime < 3)	// 3초
		return false;

	m_LastTryToConnectTime = get_global_time();

	if (m_bufevent != nullptr)
		return false;

	SPDLOG_DEBUG("SYSTEM: Trying to connect to {}:{}", m_stHost.c_str(), m_wPort);

    if (m_evbase == nullptr) {
        SPDLOG_ERROR("SYSTEM: event base not set!");
        return false;
    }
    if (m_dnsBase == nullptr) {
        SPDLOG_ERROR("SYSTEM: DNS event base not set!");
        return false;
    }

    m_bufevent = bufferevent_socket_new(m_evbase, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(m_bufevent, NULL, NULL, ClientDescEventHandler, this);
    bufferevent_enable(m_bufevent, EV_READ|EV_WRITE);
    bufferevent_socket_connect_hostname(m_bufevent, m_dnsBase, AF_UNSPEC, m_stHost.c_str(), m_wPort);

    SetPhase(PHASE_CLIENT_CONNECTING);
    return true;
}

void CLIENT_DESC::OnConnectSuccessful() {
    SPDLOG_DEBUG("SYSTEM: connected to server (ptr {})", (void*) this);
    SetPhase(m_iPhaseWhenSucceed);
}

void CLIENT_DESC::Setup(event_base* base, evdns_base * dns_base, const char * _host, WORD _port)
{
	m_evbase = base;
	m_dnsBase = dns_base;
	m_stHost = _host;
	m_wPort = _port;
	m_bufevent = nullptr;
}

void CLIENT_DESC::SetPhase(int iPhase)
{
	switch (iPhase)
	{
		case PHASE_CLIENT_CONNECTING:
			SPDLOG_DEBUG("PHASE_CLIENT_DESC::CONNECTING");
			m_pInputProcessor = NULL;
			break;

		case PHASE_DBCLIENT:
			{
				SPDLOG_DEBUG("PHASE_DBCLIENT");

				if (!g_bAuthServer)
				{
					static bool bSentBoot = false;

					if (!bSentBoot)
					{
						bSentBoot = true;
						TPacketGDBoot p;
						p.dwItemIDRange[0] = 0;
						p.dwItemIDRange[1] = 0;
						memcpy(p.szIP, g_szPublicIP.c_str(), 16);
						DBPacket(HEADER_GD_BOOT, 0, &p, sizeof(p));
					}
				}

				TEMP_BUFFER buf;

				TPacketGDSetup p;

				memset(&p, 0, sizeof(p));
				strlcpy(p.szPublicIP, g_szPublicIP.c_str(), sizeof(p.szPublicIP));
				strlcpy(p.szInternalIP, g_szInternalIP.c_str(), sizeof(p.szInternalIP));

				if (!g_bAuthServer)
				{
					p.bChannel	= g_bChannel;
					p.wListenPort = mother_port;
					p.wP2PPort	= p2p_port;
					p.bAuthServer = false;
					map_allow_copy(p.alMaps, MAP_ALLOW_MAX_LEN);

					const DESC_MANAGER::DESC_SET & c_set = DESC_MANAGER::instance().GetClientSet();
					DESC_MANAGER::DESC_SET::const_iterator it;

					for (it = c_set.begin(); it != c_set.end(); ++it)
					{
						LPDESC d = *it;

						if (d->GetAccountTable().id != 0)
							++p.dwLoginCount;
					}

					buf.write(&p, sizeof(p));

					if (p.dwLoginCount)
					{
						TPacketLoginOnSetup pck;

						for (it = c_set.begin(); it != c_set.end(); ++it)
						{
							LPDESC d = *it;

							TAccountTable & r = d->GetAccountTable();

							if (r.id != 0)
							{
								pck.dwID = r.id;
								strlcpy(pck.szLogin, r.login, sizeof(pck.szLogin));
								strlcpy(pck.szSocialID, r.social_id, sizeof(pck.szSocialID));
								strlcpy(pck.szHost, d->GetHostName(), sizeof(pck.szHost));
								pck.dwLoginKey = d->GetLoginKey();

								buf.write(&pck, sizeof(TPacketLoginOnSetup));
							}
						}
					}

					SPDLOG_DEBUG("DB_SETUP current user {} size {}", p.dwLoginCount, buf.size());

					// 파티를 처리할 수 있게 됨.
					CPartyManager::instance().EnablePCParty();
					//CPartyManager::instance().SendPartyToDB();
				}
				else
				{
					p.bAuthServer = true;
					buf.write(&p, sizeof(p));
				}

				DBPacket(HEADER_GD_SETUP, 0, buf.read_peek(), buf.size());
				m_pInputProcessor = &m_inputDB;
			}
			break;

		case PHASE_P2P:
			SPDLOG_DEBUG("PHASE_P2P");
			m_pInputProcessor = &m_inputP2P;
			break;

		case PHASE_CLOSE:
			m_pInputProcessor = NULL;
			break;

	}

	m_iPhase = iPhase;
}

void CLIENT_DESC::DBPacketHeader(BYTE bHeader, DWORD dwHandle, DWORD dwSize)
{
    DESC::RawPacket(encode_byte(bHeader), sizeof(BYTE));
    DESC::RawPacket(encode_4bytes(dwHandle), sizeof(DWORD));
    DESC::RawPacket(encode_4bytes(dwSize), sizeof(DWORD));
}

void CLIENT_DESC::DBPacket(BYTE bHeader, DWORD dwHandle, const void * c_pvData, DWORD dwSize)
{
	if (m_bufevent == nullptr) {
		SPDLOG_INFO("CLIENT_DESC [{}] trying DBPacket() while not connected",
			GetKnownClientDescName(this));
		return;
	}
	SPDLOG_TRACE("DB_PACKET: header {} handle {} size {}", bHeader, dwHandle, dwSize);
	DBPacketHeader(bHeader, dwHandle, dwSize);

	if (c_pvData)
        DESC::RawPacket(c_pvData, dwSize);
}

void CLIENT_DESC::Packet(const void * c_pvData, int iSize)
{
	if (m_bufevent == nullptr) {
		SPDLOG_INFO("CLIENT_DESC [{}] trying Packet() while not connected",
			GetKnownClientDescName(this));
		return;
	}

    DESC::RawPacket(c_pvData, iSize);
}

bool CLIENT_DESC::IsRetryWhenClosed()
{
	return (0 == thecore_is_shutdowned() && m_bRetryWhenClosed);
}

void CLIENT_DESC::Update(DWORD t)
{
	if (!g_bAuthServer) {
		UpdateChannelStatus(t, false);
	}
}

void CLIENT_DESC::UpdateChannelStatus(DWORD t, bool fForce)
{
	enum {
		CHANNELSTATUS_UPDATE_PERIOD = 5*60*1000,	// 5분마다
	};
	if (fForce || m_tLastChannelStatusUpdateTime+CHANNELSTATUS_UPDATE_PERIOD < t) {
		int iTotal; 
		int * paiEmpireUserCount;
		int iLocal;
		DESC_MANAGER::instance().GetUserCount(iTotal, &paiEmpireUserCount, iLocal);

		TChannelStatus channelStatus;
		channelStatus.nPort = mother_port;

		if (g_bNoMoreClient) channelStatus.bStatus = 0;
		else channelStatus.bStatus = iTotal > g_iFullUserCount ? 3 : iTotal > g_iBusyUserCount ? 2 : 1;

		DBPacket(HEADER_GD_UPDATE_CHANNELSTATUS, 0, &channelStatus, sizeof(channelStatus));
		m_tLastChannelStatusUpdateTime = t;
	}
}

void CLIENT_DESC::Reset()
{
	// Backup connection target info
	event_base * evbase = m_evbase;
	std::string host = m_stHost;
	WORD port = m_wPort;

	Destroy();
	Initialize();

	// Restore connection target info
    m_evbase = evbase;
	m_stHost = host;
	m_wPort = port;
}
