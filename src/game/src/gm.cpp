#include "stdafx.h"
#include "constants.h"
#include "gm.h"
#include "locale_service.h"

extern int test_server;

//ADMIN_MANAGER
typedef struct 
{
	tAdminInfo Info;		
	std::set<std::string> *pset_Host;	
} tGM;

std::set<std::string> g_set_Host;
std::map<std::string, tGM> g_map_GM;

void gm_new_clear()
{
	g_set_Host.clear();
	g_map_GM.clear();
}

void gm_new_insert( const tAdminInfo &rAdminInfo )
{	
	SPDLOG_DEBUG("InsertGMList(account:{}, player:{}, contact_ip:{}, server_ip:{}, auth:{})",
			rAdminInfo.m_szAccount,
			rAdminInfo.m_szName,
			rAdminInfo.m_szContactIP,
			rAdminInfo.m_szServerIP,
			rAdminInfo.m_Authority);

	tGM t;

	if ( strlen( rAdminInfo.m_szContactIP ) == 0 )
	{
		t.pset_Host = &g_set_Host;
		SPDLOG_DEBUG("GM Use ContactIP" );
	}
	else
	{
		t.pset_Host = NULL;
		SPDLOG_DEBUG("GM Use Default Host List" );
	}

	memcpy ( &t.Info, &rAdminInfo, sizeof ( rAdminInfo ) );

	g_map_GM[rAdminInfo.m_szName] = t;

}

void gm_new_host_inert( const char * host )
{
	g_set_Host.insert( host );
	SPDLOG_DEBUG("InsertGMHost(ip:{})", host );
}

BYTE gm_new_get_level( const char * name, const char * host, const char* account)
{
	if ( test_server ) return GM_IMPLEMENTOR;

	std::map<std::string, tGM >::iterator it = g_map_GM.find(name);

	if (g_map_GM.end() == it)
		return GM_PLAYER;

	if (account)
	{
		if ( strcmp ( it->second.Info.m_szAccount, account	) != 0 )
		{
				SPDLOG_DEBUG("GM_NEW_GET_LEVEL : BAD ACCOUNT [ACCOUNT:{}/{}", it->second.Info.m_szAccount, account);
				return GM_PLAYER;
		}
	}
	SPDLOG_DEBUG("GM_NEW_GET_LEVEL : FIND ACCOUNT");
	return it->second.Info.m_Authority;
}
	
//END_ADMIN_MANAGER
BYTE gm_get_level(const char * name, const char * host, const char* account)
{
	return gm_new_get_level( name, host, account );
}

