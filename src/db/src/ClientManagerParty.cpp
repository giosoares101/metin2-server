// vim:ts=4 sw=4
#include "stdafx.h"
#include "ClientManager.h"
#include "Config.h"
#include "DBManager.h"
#include "QID.h"

void CClientManager::QUERY_PARTY_CREATE(CPeer* peer, TPacketPartyCreate* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];

	if (pm.find(p->dwLeaderPID) == pm.end())
	{
		pm.insert(make_pair(p->dwLeaderPID, TPartyMember()));
		ForwardPacket(HEADER_DG_PARTY_CREATE, p, sizeof(TPacketPartyCreate), peer->GetChannel(), peer);
		SPDLOG_DEBUG("PARTY Create [{}]", p->dwLeaderPID);
	}
	else
	{
		SPDLOG_ERROR("PARTY Create - Already exists [{}]", p->dwLeaderPID);
	}
}

void CClientManager::QUERY_PARTY_DELETE(CPeer* peer, TPacketPartyDelete* p)
{
	TPartyMap& pm = m_map_pkChannelParty[peer->GetChannel()];
	itertype(pm) it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		SPDLOG_ERROR("PARTY Delete - Non exists [{}]", p->dwLeaderPID);
		return;
	}

	pm.erase(it);
	ForwardPacket(HEADER_DG_PARTY_DELETE, p, sizeof(TPacketPartyDelete), peer->GetChannel(), peer);
	SPDLOG_DEBUG("PARTY Delete [{}]", p->dwLeaderPID);
}

void CClientManager::QUERY_PARTY_ADD(CPeer* peer, TPacketPartyAdd* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	itertype(pm) it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		SPDLOG_ERROR("PARTY Add - Non exists [{}]", p->dwLeaderPID);
		return;
	}

	if (it->second.find(p->dwPID) == it->second.end())
	{
		it->second.insert(std::make_pair(p->dwPID, TPartyInfo()));
		ForwardPacket(HEADER_DG_PARTY_ADD, p, sizeof(TPacketPartyAdd), peer->GetChannel(), peer);
		SPDLOG_DEBUG("PARTY Add [{}] to [{}]", p->dwPID, p->dwLeaderPID);
	}
	else
		SPDLOG_ERROR("PARTY Add - Already [{}] in party [{}]", p->dwPID, p->dwLeaderPID);
}

void CClientManager::QUERY_PARTY_REMOVE(CPeer* peer, TPacketPartyRemove* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	itertype(pm) it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		SPDLOG_ERROR("PARTY Remove - Non exists [{}] cannot remove [{}]",p->dwLeaderPID, p->dwPID);
		return;
	}

	itertype(it->second) pit = it->second.find(p->dwPID);

	if (pit != it->second.end())
	{
		it->second.erase(pit);
		ForwardPacket(HEADER_DG_PARTY_REMOVE, p, sizeof(TPacketPartyRemove), peer->GetChannel(), peer);
		SPDLOG_DEBUG("PARTY Remove [{}] to [{}]", p->dwPID, p->dwLeaderPID);
	}
	else
		SPDLOG_ERROR("PARTY Remove - Cannot find [{}] in party [{}]", p->dwPID, p->dwLeaderPID);
}

void CClientManager::QUERY_PARTY_STATE_CHANGE(CPeer* peer, TPacketPartyStateChange* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	itertype(pm) it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		SPDLOG_ERROR("PARTY StateChange - Non exists [{}] cannot state change [{}]",p->dwLeaderPID, p->dwPID);
		return;
	}

	itertype(it->second) pit = it->second.find(p->dwPID);

	if (pit == it->second.end())
	{
		SPDLOG_ERROR("PARTY StateChange - Cannot find [{}] in party [{}]", p->dwPID, p->dwLeaderPID);
		return;
	}

	if (p->bFlag)
		pit->second.bRole = p->bRole;
	else 
		pit->second.bRole = 0;

	ForwardPacket(HEADER_DG_PARTY_STATE_CHANGE, p, sizeof(TPacketPartyStateChange), peer->GetChannel(), peer);
	SPDLOG_DEBUG("PARTY StateChange [{}] at [{}] from {} {}",p->dwPID, p->dwLeaderPID, p->bRole, p->bFlag);
}

void CClientManager::QUERY_PARTY_SET_MEMBER_LEVEL(CPeer* peer, TPacketPartySetMemberLevel* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	itertype(pm) it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		SPDLOG_ERROR("PARTY SetMemberLevel - Non exists [{}] cannot level change [{}]",p->dwLeaderPID, p->dwPID);
		return;
	}

	itertype(it->second) pit = it->second.find(p->dwPID);

	if (pit == it->second.end())
	{
		SPDLOG_ERROR("PARTY SetMemberLevel - Cannot find [{}] in party [{}]", p->dwPID, p->dwLeaderPID);
		return;
	}

	pit->second.bLevel = p->bLevel;

	ForwardPacket(HEADER_DG_PARTY_SET_MEMBER_LEVEL, p, sizeof(TPacketPartySetMemberLevel), peer->GetChannel());
	SPDLOG_DEBUG("PARTY SetMemberLevel pid [{}] level {}",p->dwPID, p->bLevel);
}
