#include "stdafx.h" 
#include "constants.h"
#include "config.h"
#include "input.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "db.h"

extern time_t get_global_time();

bool FN_IS_VALID_LOGIN_STRING(const char *str)
{
	const char*	tmp;

	if (!str || !*str)
		return false;

	if (strlen(str) < 2)
		return false;

	for (tmp = str; *tmp; ++tmp)
	{
		// 알파벳과 수자만 허용
		if (isdigit(*tmp) || isalpha(*tmp))
			continue;

		return false;
	}

	return true;
}

CInputAuth::CInputAuth()
{
}

void CInputAuth::Login(LPDESC d, const char * c_pData)
{
	TPacketCGLogin3 * pinfo = (TPacketCGLogin3 *) c_pData;

	if (!g_bAuthServer)
	{
		SPDLOG_ERROR("CInputAuth class is not for game server. IP {} might be a hacker.", d->GetHostName());
		d->DelayedDisconnect(5);
		return;
	}

	// Copy for string integrity
	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(pinfo->login, login, sizeof(login));

	char passwd[PASSWD_MAX_LEN + 1];
	strlcpy(passwd, pinfo->passwd, sizeof(passwd));

	SPDLOG_DEBUG("InputAuth::Login : {}({}) desc {}",
			login, strlen(login), (void*) get_pointer(d));

	// check login string
	if (false == FN_IS_VALID_LOGIN_STRING(login))
	{
		SPDLOG_DEBUG("InputAuth::Login : IS_NOT_VALID_LOGIN_STRING({}) desc {}",
				login, (void*) get_pointer(d));
		LoginFailure(d, "WRONGCRD");
		return;
	}

	if (g_bNoMoreClient)
	{
		TPacketGCLoginFailure failurePacket;

		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.szStatus, "SHUTDOWN", sizeof(failurePacket.szStatus));

		d->Packet(&failurePacket, sizeof(failurePacket));
		return;
	}

	if (DESC_MANAGER::instance().FindByLoginName(login))
	{
		LoginFailure(d, "ALREADY");
		return;
	}

	DWORD dwKey = DESC_MANAGER::instance().CreateLoginKey(d);
	DWORD dwPanamaKey = dwKey ^ pinfo->adwClientKey[0] ^ pinfo->adwClientKey[1] ^ pinfo->adwClientKey[2] ^ pinfo->adwClientKey[3];
	d->SetPanamaKey(dwPanamaKey);

	SPDLOG_DEBUG("InputAuth::Login : key {}:{} login {}", dwKey, dwPanamaKey, login);

	TPacketCGLogin3 * p = M2_NEW TPacketCGLogin3;
	memcpy(p, pinfo, sizeof(TPacketCGLogin3));

	char szLogin[LOGIN_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(szLogin, sizeof(szLogin), login, strlen(login));

	DBManager::instance().ReturnQuery(QID_AUTH_LOGIN, dwKey, p, 
			"SELECT password,securitycode,social_id,id,status,availDt - NOW() > 0,"
			"UNIX_TIMESTAMP(silver_expire),"
			"UNIX_TIMESTAMP(gold_expire),"
			"UNIX_TIMESTAMP(safebox_expire),"
			"UNIX_TIMESTAMP(autoloot_expire),"
			"UNIX_TIMESTAMP(fish_mind_expire),"
			"UNIX_TIMESTAMP(marriage_fast_expire),"
			"UNIX_TIMESTAMP(money_drop_rate_expire),"
			"UNIX_TIMESTAMP(create_time)"
			" FROM account WHERE login='%s'",
			szLogin);
}

int CInputAuth::Analyze(LPDESC d, BYTE bHeader, const char * c_pData)
{

	if (!g_bAuthServer)
	{
		SPDLOG_ERROR("CInputAuth class is not for game server. IP {} might be a hacker.", d->GetHostName());
		d->DelayedDisconnect(5);
		return 0;
	}

	int iExtraLen = 0;

    SPDLOG_TRACE(" InputAuth Analyze Header[{}] ", bHeader);

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			Pong(d);
			break;

		case HEADER_CG_LOGIN3:
			Login(d, c_pData);
			break;

		case HEADER_CG_HANDSHAKE:
			break;

		default:
			SPDLOG_ERROR("This phase does not handle this header {} (0x{})(phase: AUTH)", bHeader, bHeader);
			break;
	}

	return iExtraLen;
}
