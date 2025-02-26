
#include "stdafx.h"

#include "map_location.h"

#include "sectree_manager.h"

CMapLocation g_mapLocations;

bool CMapLocation::Get(int x, int y, int & lIndex, LONG & lAddr, WORD & wPort)
{
	lIndex = SECTREE_MANAGER::instance().GetMapIndex(x, y);

	return Get(lIndex, lAddr, wPort);
}

bool CMapLocation::Get(int iIndex, LONG & lAddr, WORD & wPort)
{
	if (iIndex == 0)
	{
		SPDLOG_ERROR("CMapLocation::Get - Error MapIndex[{}]", iIndex);
		return false;
	}

	std::map<int, TLocation>::iterator it = m_map_address.find(iIndex);

	if (m_map_address.end() == it)
	{
		SPDLOG_ERROR("CMapLocation::Get - Error MapIndex[{}]", iIndex);
		std::map<int, TLocation>::iterator i;
		for ( i	= m_map_address.begin(); i != m_map_address.end(); ++i)
		{
			SPDLOG_ERROR("Map({}): Server({}:{})", i->first, i->second.addr, i->second.port);
		}
		return false;
	}

	lAddr = it->second.addr;
	wPort = it->second.port;
	return true;
}

void CMapLocation::Insert(int lIndex, const char * c_pszHost, WORD wPort)
{
	TLocation loc;

	loc.addr = inet_addr(c_pszHost);
	loc.port = wPort;

	m_map_address.insert(std::make_pair(lIndex, loc));
	SPDLOG_DEBUG("MapLocation::Insert : {} {} {}", lIndex, c_pszHost, wPort);
}

