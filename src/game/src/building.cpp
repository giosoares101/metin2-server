#include "stdafx.h"
#include "constants.h"
#include "sectree_manager.h"
#include "item_manager.h"
#include "buffer_manager.h"
#include "config.h"
#include "packet.h"
#include "char.h"
#include "char_manager.h"
#include "guild.h"
#include "guild_manager.h"
#include "desc.h"
#include "desc_manager.h"
#include "desc_client.h"
#include "questmanager.h"
#include "building.h"

enum
{
	// ADD_SUPPLY_BUILDING
	BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL = 14061,
	BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM = 14062,
	BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE = 14063,
	// END_OF_ADD_SUPPLY_BUILDING

	FLAG_VNUM = 14200,
	WALL_DOOR_VNUM	= 14201,
	WALL_BACK_VNUM	= 14202,
	WALL_LEFT_VNUM	= 14203,
	WALL_RIGHT_VNUM	= 14204,
};

using namespace building;

CObject::CObject(TObject * pData, TObjectProto * pProto)
	: m_pProto(pProto), m_dwVID(0), m_chNPC(NULL)
{
	CEntity::Initialize(ENTITY_OBJECT);

	memcpy(&m_data, pData, sizeof(TObject));
}

CObject::~CObject()
{
	Destroy();
}

void CObject::Destroy()
{
	if (m_pProto)
	{
		SECTREE_MANAGER::instance().ForAttrRegion(GetMapIndex(),
				GetX() + m_pProto->lRegion[0],
				GetY() + m_pProto->lRegion[1],
				GetX() + m_pProto->lRegion[2],
				GetY() + m_pProto->lRegion[3],
				(int)m_data.zRot, // ADD_BUILDING_ROTATION
				ATTR_OBJECT,
				ATTR_REGION_MODE_REMOVE);
	}

	CEntity::Destroy();

	if (GetSectree())
		GetSectree()->RemoveEntity(this);

	// <Factor> NPC should be destroyed in CHARACTER_MANAGER
	// BUILDING_NPC
	/*
	if (m_chNPC) {
		M2_DESTROY_CHARACTER(m_chNPC);
	}
	*/

	RemoveSpecialEffect();
	// END_OF_BUILDING_NPC
}

// BUILDING_NPC
void CObject::Reconstruct(DWORD dwVnum)
{
	const TMapRegion * r = SECTREE_MANAGER::instance().GetMapRegion(m_data.lMapIndex);
	if (!r)
		return;

	CLand* pLand = GetLand();
	pLand->RequestDeleteObject(GetID());
	pLand->RequestCreateObject(dwVnum, m_data.lMapIndex, m_data.x - r->sx, m_data.y - r->sy, m_data.xRot, m_data.yRot, m_data.zRot, false);
}
// END_OF_BUILDING_NPC

void CObject::EncodeInsertPacket(LPENTITY entity)
{
	LPDESC d;

	if (!(d = entity->GetDesc()))
		return;

	SPDLOG_DEBUG("ObjectInsertPacket vid {} vnum {} rot {} {} {}",
			m_dwVID, m_data.dwVnum, m_data.xRot, m_data.yRot, m_data.zRot);

	TPacketGCCharacterAdd pack;

	memset(&pack, 0, sizeof(TPacketGCCharacterAdd));

	pack.header         = HEADER_GC_CHARACTER_ADD;
	pack.dwVID          = m_dwVID;
	pack.bType          = CHAR_TYPE_BUILDING;
	pack.angle          = m_data.zRot;
	pack.x              = GetX();
	pack.y              = GetY();
	pack.z              = GetZ();
	pack.wRaceNum       = m_data.dwVnum;

	// 빌딩 회전 정보(벽일때는 문 위치)를 변환
	pack.dwAffectFlag[0] = unsigned(m_data.xRot);
	pack.dwAffectFlag[1] = unsigned(m_data.yRot);


	if (GetLand())
	{
		// pack.dwGuild = GetLand()->GetOwner();
	}

	d->Packet(&pack, sizeof(pack));
}

void CObject::EncodeRemovePacket(LPENTITY entity)
{
	LPDESC d;

	if (!(d = entity->GetDesc()))
		return;

	SPDLOG_DEBUG("ObjectRemovePacket vid {}", m_dwVID);

	TPacketGCCharacterDelete pack;

	pack.header = HEADER_GC_CHARACTER_DEL;
	pack.id     = m_dwVID;

	d->Packet(&pack, sizeof(TPacketGCCharacterDelete));
}

void CObject::SetVID(DWORD dwVID)
{
	m_dwVID = dwVID;
}

bool CObject::Show(int lMapIndex, int x, int y)
{
	LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);

	if (!tree)
	{
		SPDLOG_ERROR("cannot find sectree by {}x{} mapindex {}", x, y, lMapIndex);
		return false;
	}

	if (GetSectree())
	{
		GetSectree()->RemoveEntity(this);
		ViewCleanup();
	}

	m_data.lMapIndex = lMapIndex;
	m_data.x = x;
	m_data.y = y;

	Save();

	SetMapIndex(lMapIndex);
	SetXYZ(x, y, 0);

	tree->InsertEntity(this);
	UpdateSectree();

	SECTREE_MANAGER::instance().ForAttrRegion(lMapIndex,
			x + m_pProto->lRegion[0],
			y + m_pProto->lRegion[1],
			x + m_pProto->lRegion[2],
			y + m_pProto->lRegion[3],
			(int)m_data.zRot,
			ATTR_OBJECT,
			ATTR_REGION_MODE_SET);

	return true;
}

void CObject::Save()
{
}

void CObject::ApplySpecialEffect()
{
	if (m_pProto)
	{
		// ADD_SUPPLY_BUILDING
		if (m_pProto->dwVnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL ||
				m_pProto->dwVnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM ||
				m_pProto->dwVnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE)
		{
			CLand* pLand = GetLand();
			DWORD guild_id = 0;
			if (pLand)
				guild_id = pLand->GetOwner();
			CGuild* pGuild = CGuildManager::instance().FindGuild(guild_id);
			if (pGuild)
			{
				switch (m_pProto->dwVnum)
				{
					case BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL:
						pGuild->SetMemberCountBonus(6);
						break;
					case BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM:
						pGuild->SetMemberCountBonus(12);
						break;
					case BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE:
						pGuild->SetMemberCountBonus(18);
						break;
				}
				if (map_allow_find(pLand->GetMapIndex()))
				{
					pGuild->BroadcastMemberCountBonus();
				}
			}
		}
		// END_OF_ADD_SUPPLY_BUILDING
	}
}

void CObject::RemoveSpecialEffect()
{
	if (m_pProto)
	{
		// ADD_SUPPLY_BUILDING
		if (m_pProto->dwVnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL ||
				m_pProto->dwVnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM ||
				m_pProto->dwVnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE)
		{
			CLand* pLand = GetLand();
			DWORD guild_id = 0;
			if (pLand)
				guild_id = pLand->GetOwner();
			CGuild* pGuild = CGuildManager::instance().FindGuild(guild_id);
			if (pGuild)
			{
				pGuild->SetMemberCountBonus(0);
				if (map_allow_find(pLand->GetMapIndex()))
					pGuild->BroadcastMemberCountBonus();
			}
		}
		// END_OF_ADD_SUPPLY_BUILDING
	}
}

// BUILDING_NPC
void CObject::RegenNPC()
{
	if (!m_pProto)
		return;

	if (!m_pProto->dwNPCVnum)
		return;

	if (!m_pkLand)
		return;

	DWORD dwGuildID = m_pkLand->GetOwner();
	CGuild* pGuild = CGuildManager::instance().FindGuild(dwGuildID);

	if (!pGuild)
		return;

	int x = m_pProto->lNPCX;
	int y = m_pProto->lNPCY;
	int newX, newY;

	float rot = m_data.zRot * 2.0f * M_PI / 360.0f;

	newX = (int)(( x * cosf(rot)) + ( y * sinf(rot)));
	newY = (int)(( y * cosf(rot)) - ( x * sinf(rot)));

	m_chNPC = CHARACTER_MANAGER::instance().SpawnMob(m_pProto->dwNPCVnum,
			GetMapIndex(),
			GetX() + newX,
			GetY() + newY,
			GetZ(),
			false,
			(int)m_data.zRot);


	if (!m_chNPC)
	{
		SPDLOG_ERROR("Cannot create guild npc");
		return;
	}

	m_chNPC->SetGuild(pGuild);

	// 힘의 신전일 경우 길드 레벨을 길마에게 저장해놓는다
	if ( m_pProto->dwVnum == 14061 || m_pProto->dwVnum == 14062 || m_pProto->dwVnum == 14063 )
	{
		quest::PC* pPC = quest::CQuestManager::instance().GetPC(pGuild->GetMasterPID());

		if ( pPC != NULL )
		{
			pPC->SetFlag("alter_of_power.build_level", pGuild->GetLevel());
		}
	}
}
// END_OF_BUILDING_NPC

////////////////////////////////////////////////////////////////////////////////////

CLand::CLand(TLand * pData)
{
	memcpy(&m_data, pData, sizeof(TLand));
}

CLand::~CLand()
{
	Destroy();
}

void CLand::Destroy()
{
	itertype(m_map_pkObject) it = m_map_pkObject.begin();

	while (it != m_map_pkObject.end())
	{
		LPOBJECT pkObj = (it++)->second;
		CManager::instance().UnregisterObject(pkObj);
		M2_DELETE(pkObj);
	}

	m_map_pkObject.clear();
	m_map_pkObjectByVID.clear();
}

const TLand & CLand::GetData()
{
	return m_data;
}

void CLand::PutData(const TLand * data)
{
	memcpy(&m_data, data, sizeof(TLand));

	if (m_data.dwGuildID)
	{
		const TMapRegion * r = SECTREE_MANAGER::instance().GetMapRegion(m_data.lMapIndex);

		if (r)
		{
			CharacterVectorInteractor i;

			if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(20040, i))
			{
				CharacterVectorInteractor::iterator it = i.begin();

				while (it != i.end())
				{
					LPCHARACTER ch = *(it++);

					if (ch->GetMapIndex() != m_data.lMapIndex)
						continue;

					int x = ch->GetX() - r->sx;
					int y = ch->GetY() - r->sy;

					if (x > m_data.x + m_data.width || x < m_data.x)
						continue;

					if (y > m_data.y + m_data.height || y < m_data.y)
						continue;

					M2_DESTROY_CHARACTER(ch);
				}
			}
		}
	}
}

void CLand::InsertObject(LPOBJECT pkObj)
{
	m_map_pkObject.insert(std::make_pair(pkObj->GetID(), pkObj));
	m_map_pkObjectByVID.insert(std::make_pair(pkObj->GetVID(), pkObj));

	pkObj->SetLand(this);
}

LPOBJECT CLand::FindObject(DWORD dwID)
{
	std::map<DWORD, LPOBJECT>::iterator it = m_map_pkObject.find(dwID);

	if (it == m_map_pkObject.end())
		return NULL;

	return it->second;
}

LPOBJECT CLand::FindObjectByGroup(DWORD dwGroupVnum)
{
	std::map<DWORD, LPOBJECT>::iterator it;
	for (it = m_map_pkObject.begin(); it != m_map_pkObject.end(); ++it)
	{
		LPOBJECT pObj = it->second;
		if (pObj->GetGroup() == dwGroupVnum)
			return pObj;
	}

	return NULL;
}

LPOBJECT CLand::FindObjectByVnum(DWORD dwVnum)
{
	std::map<DWORD, LPOBJECT>::iterator it;
	for (it = m_map_pkObject.begin(); it != m_map_pkObject.end(); ++it)
	{
		LPOBJECT pObj = it->second;
		if (pObj->GetVnum() == dwVnum)
			return pObj;
	}

	return NULL;
}

// BUILDING_NPC
LPOBJECT CLand::FindObjectByNPC(LPCHARACTER npc)
{
	if (!npc)
		return NULL;

	std::map<DWORD, LPOBJECT>::iterator it;
	for (it = m_map_pkObject.begin(); it != m_map_pkObject.end(); ++it)
	{
		LPOBJECT pObj = it->second;
		if (pObj->GetNPC() == npc)
			return pObj;
	}

	return NULL;
}
// END_OF_BUILDING_NPC

LPOBJECT CLand::FindObjectByVID(DWORD dwVID)
{
	std::map<DWORD, LPOBJECT>::iterator it = m_map_pkObjectByVID.find(dwVID);

	if (it == m_map_pkObjectByVID.end())
		return NULL;

	return it->second;
}

void CLand::DeleteObject(DWORD dwID)
{
	LPOBJECT pkObj;

	if (!(pkObj = FindObject(dwID)))
		return;

	SPDLOG_DEBUG("Land::DeleteObject {}", dwID);
	CManager::instance().UnregisterObject(pkObj);
	M2_DESTROY_CHARACTER (pkObj->GetNPC());

	m_map_pkObject.erase(dwID);
	m_map_pkObjectByVID.erase(dwID);

	M2_DELETE(pkObj);
}

struct FIsIn
{
	int sx, sy;
	int ex, ey;
	
	bool bIn;
	FIsIn (	int sx_, int sy_, int ex_, int ey_)
		: sx(sx_), sy(sy_), ex(ex_), ey(ey_), bIn(false) 
	{}

	void operator () (LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER) ent;
			if (ch->IsMonster())
			{
				return;
			}
			if (sx <= ch->GetX() && ch->GetX() <= ex
				&& sy <= ch->GetY() && ch->GetY() <= ey)
			{
				bIn = true;
			}
		}
	}
};

bool CLand::RequestCreateObject(DWORD dwVnum, int lMapIndex, int x, int y, float xRot, float yRot, float zRot, bool checkAnother)
{
	SECTREE_MANAGER& rkSecTreeMgr = SECTREE_MANAGER::instance();
	TObjectProto * pkProto = CManager::instance().GetObjectProto(dwVnum);

	if (!pkProto)
	{
		SPDLOG_ERROR("Invalid Object vnum {}", dwVnum);
		return false;
	}
	const TMapRegion * r = rkSecTreeMgr.GetMapRegion(lMapIndex);
	if (!r)
		return false;

	SPDLOG_DEBUG("RequestCreateObject(vnum={}, map={}, pos=({},{}), rot=({:.1f},{:.1f},{:.1f}) region({},{} ~ {},{})",
			dwVnum, lMapIndex, x, y, xRot, yRot, zRot, r->sx, r->sy, r->ex, r->ey);

	x += r->sx;
	y += r->sy;

	int sx = r->sx + m_data.x;
	int ex = sx + m_data.width;
	int sy = r->sy + m_data.y;
	int ey = sy + m_data.height;

	int osx = x + pkProto->lRegion[0];
	int osy = y + pkProto->lRegion[1];
	int oex = x + pkProto->lRegion[2];
	int oey = y + pkProto->lRegion[3];

	float rad = zRot * 2.0f * M_PI / 360.0f;

	int tsx = (int)(pkProto->lRegion[0] * cosf(rad) + pkProto->lRegion[1] * sinf(rad) + x);
	int tsy = (int)(pkProto->lRegion[0] * -sinf(rad) + pkProto->lRegion[1] * cosf(rad) + y);

	int tex = (int)(pkProto->lRegion[2] * cosf(rad) + pkProto->lRegion[3] * sinf(rad) + x);
	int tey = (int)(pkProto->lRegion[2] * -sinf(rad) + pkProto->lRegion[3] * cosf(rad) + y);

	if (tsx < sx || tex > ex || tsy < sy || tey > ey)
	{
		SPDLOG_ERROR("invalid position: object is outside of land region\nLAND: {} {} ~ {} {}\nOBJ: {} {} ~ {} {}", sx, sy, ex, ey, osx, osy, oex, oey);
		return false;
	}

	// ADD_BUILDING_ROTATION
	if ( checkAnother )
	{
		if (rkSecTreeMgr.ForAttrRegion(lMapIndex, osx, osy, oex, oey, (int)zRot, ATTR_OBJECT, ATTR_REGION_MODE_CHECK))
		{
			SPDLOG_ERROR("another object already exist");
			return false;
		}
		FIsIn f (osx, osy, oex, oey);
		rkSecTreeMgr.GetMap(lMapIndex)->for_each(f);
		
		if (f.bIn)
		{
			SPDLOG_ERROR("another object already exist");
			return false;
		}
	}
	// END_OF_BUILDING_NPC

	TPacketGDCreateObject p;

	p.dwVnum = dwVnum;
	p.dwLandID = m_data.dwID;
	p.lMapIndex = lMapIndex;
	p.x = x;
	p.y = y;
	p.xRot = xRot;
	p.yRot = yRot;
	p.zRot = zRot;

	db_clientdesc->DBPacket(HEADER_GD_CREATE_OBJECT, 0, &p, sizeof(TPacketGDCreateObject));
	return true;
}

void CLand::RequestDeleteObject(DWORD dwID)
{
	if (!FindObject(dwID))
	{
		SPDLOG_ERROR("no object by id {}", dwID);
		return;
	}

	db_clientdesc->DBPacket(HEADER_GD_DELETE_OBJECT, 0, &dwID, sizeof(DWORD));
	SPDLOG_DEBUG("RequestDeleteObject id {}", dwID);
}

void CLand::RequestDeleteObjectByVID(DWORD dwVID)
{
	LPOBJECT pkObj;

	if (!(pkObj = FindObjectByVID(dwVID)))
	{
		SPDLOG_ERROR("no object by vid {}", dwVID);
		return;
	}

	DWORD dwID = pkObj->GetID();
	db_clientdesc->DBPacket(HEADER_GD_DELETE_OBJECT, 0, &dwID, sizeof(DWORD));
	SPDLOG_DEBUG("RequestDeleteObject vid {} id {}", dwVID, dwID);
}

void CLand::SetOwner(DWORD dwGuild)
{
	if (m_data.dwGuildID != dwGuild)
	{
		m_data.dwGuildID = dwGuild;
		RequestUpdate(dwGuild);
	}
}

void CLand::RequestUpdate(DWORD dwGuild)
{
	DWORD a[2];

	a[0] = GetID();
	a[1] = dwGuild;

	db_clientdesc->DBPacket(HEADER_GD_UPDATE_LAND, 0, &a[0], sizeof(DWORD) * 2);
	SPDLOG_DEBUG("RequestUpdate id {} guild {}", a[0], a[1]);
}

////////////////////////////////////////////////////////////////////////////////////

CManager::CManager()
{
}

CManager::~CManager()
{
	Destroy();
}

void CManager::Destroy()
{
	itertype(m_map_pkLand) it = m_map_pkLand.begin();
	for ( ; it != m_map_pkLand.end(); ++it) {
		M2_DELETE(it->second);
	}
	m_map_pkLand.clear();
}

bool CManager::LoadObjectProto(const TObjectProto * pProto, int size) // from DB
{
	m_vec_kObjectProto.resize(size);
	memcpy(&m_vec_kObjectProto[0], pProto, sizeof(TObjectProto) * size);

	for (int i = 0; i < size; ++i)
	{
		TObjectProto & r = m_vec_kObjectProto[i];

		// BUILDING_NPC
		SPDLOG_DEBUG("ObjectProto {} price {} upgrade {} upg_limit {} life {} NPC {}",
				r.dwVnum, r.dwPrice, r.dwUpgradeVnum, r.dwUpgradeLimitTime, r.lLife, r.dwNPCVnum);
		// END_OF_BUILDING_NPC

		for (int j = 0; j < OBJECT_MATERIAL_MAX_NUM; ++j)
		{
			if (!r.kMaterials[j].dwItemVnum)
				break;

			if (NULL == ITEM_MANAGER::instance().GetTable(r.kMaterials[j].dwItemVnum))
			{
				SPDLOG_ERROR("          mat: ERROR!! no item by vnum {}", r.kMaterials[j].dwItemVnum);
				return false;
			}

            SPDLOG_TRACE("          mat: {} {}", r.kMaterials[j].dwItemVnum, r.kMaterials[j].dwCount);
		}

		m_map_pkObjectProto.insert(std::make_pair(r.dwVnum, &m_vec_kObjectProto[i]));
	}

	return true;
}

TObjectProto * CManager::GetObjectProto(DWORD dwVnum)
{
	itertype(m_map_pkObjectProto) it = m_map_pkObjectProto.find(dwVnum);

	if (it == m_map_pkObjectProto.end())
		return NULL;

	return it->second;
}

bool CManager::LoadLand(TLand * pTable) // from DB
{
	// MapAllow에 없는 맵의 땅일지라도 load를 해야한다.
	//	건물(object)이 어느 길드에 속해 있는지 알기 위해서는 건물이 세위진 땅이 어느 길드 소속인지 알아한다.
	//	만약 땅을 load해 놓지 않으면 길드 건물이 어느 길드에 소속된 건지 알지 못해서
	//	길드 건물에 의한 길드 버프를 받지 못한다.
	//if (!map_allow_find(pTable->lMapIndex))
	//	return false;

	CLand * pkLand = M2_NEW CLand(pTable);
	m_map_pkLand.insert(std::make_pair(pkLand->GetID(), pkLand));

	SPDLOG_INFO("LAND: {} map {} {}x{} w {} h {}",
			pTable->dwID, pTable->lMapIndex, pTable->x, pTable->y, pTable->width, pTable->height);

	return true;
}

void CManager::UpdateLand(TLand * pTable)
{
	CLand * pkLand = FindLand(pTable->dwID);

	if (!pkLand)
	{
		SPDLOG_ERROR("cannot find land by id {}", pTable->dwID);
		return;
	}

	pkLand->PutData(pTable);

	const DESC_MANAGER::DESC_SET & cont = DESC_MANAGER::instance().GetClientSet();

	itertype(cont) it = cont.begin();

	TPacketGCLandList p;

	p.header = HEADER_GC_LAND_LIST;
	p.size = sizeof(TPacketGCLandList) + sizeof(TLandPacketElement);

	TLandPacketElement e;

	e.dwID = pTable->dwID;
	e.x = pTable->x;
	e.y = pTable->y;
	e.width = pTable->width;
	e.height = pTable->height;
	e.dwGuildID = pTable->dwGuildID;

	SPDLOG_INFO("BUILDING: UpdateLand {} pos {}x{} guild {}", e.dwID, e.x, e.y, e.dwGuildID);

	CGuild *guild = CGuildManager::instance().FindGuild(pTable->dwGuildID);
	while (it != cont.end())
	{
		LPDESC d = *(it++);

		if (d->GetCharacter() && d->GetCharacter()->GetMapIndex() == pTable->lMapIndex)
		{
			// we must send the guild name first
			d->GetCharacter()->SendGuildName(guild);

            d->RawPacket(&p, sizeof(TPacketGCLandList));
			d->Packet(&e, sizeof(TLandPacketElement));
		}
	}
}

CLand * CManager::FindLand(DWORD dwID)
{
	std::map<DWORD, CLand *>::iterator it = m_map_pkLand.find(dwID);

	if (it == m_map_pkLand.end())
		return NULL;

	return it->second;
}

CLand * CManager::FindLand(int lMapIndex, int x, int y)
{
	SPDLOG_DEBUG("BUILDING: FindLand {} {} {}", lMapIndex, x, y);

	const TMapRegion * r = SECTREE_MANAGER::instance().GetMapRegion(lMapIndex);

	if (!r)
		return NULL;

	x -= r->sx;
	y -= r->sy;

	itertype(m_map_pkLand) it = m_map_pkLand.begin();

	while (it != m_map_pkLand.end())
	{
		CLand * pkLand = (it++)->second;
		const TLand & r = pkLand->GetData();

		if (r.lMapIndex != lMapIndex)
			continue;

		if (x < r.x || y < r.y)
			continue;

		if (x > r.x + r.width || y > r.y + r.height)
			continue;

		return pkLand;
	}

	return NULL;
}

CLand * CManager::FindLandByGuild(DWORD GID)
{
	itertype(m_map_pkLand) it = m_map_pkLand.begin();

	while (it != m_map_pkLand.end())
	{
		CLand * pkLand = (it++)->second;

		if (pkLand->GetData().dwGuildID == GID)
			return pkLand;
	}

	return NULL;
}

bool CManager::LoadObject(TObject * pTable, bool isBoot) // from DB
{
	// Skip loading if the target map is not available on this core
	// Source: https://metin2.dev/topic/23614-solving-a-very-old-problem-what-most-people-ignore/#comment-163073
	if (!map_allow_find(pTable->lMapIndex))
		return false;

	CLand * pkLand = FindLand(pTable->dwLandID);

	if (!pkLand)
	{
		SPDLOG_ERROR("Cannot find land by id {}", pTable->dwLandID);
		return false;
	}

	TObjectProto * pkProto = GetObjectProto(pTable->dwVnum);

	if (!pkProto)
	{
		SPDLOG_ERROR("Cannot find object {} in prototype (id {})", pTable->dwVnum, pTable->dwID);
		return false;
	}

	SPDLOG_DEBUG("OBJ: id {} vnum {} map {} pos {}x{}", pTable->dwID, pTable->dwVnum, pTable->lMapIndex, pTable->x, pTable->y);

	LPOBJECT pkObj = M2_NEW CObject(pTable, pkProto);

	DWORD dwVID = CHARACTER_MANAGER::instance().AllocVID();
	pkObj->SetVID(dwVID);

	m_map_pkObjByVID.insert(std::make_pair(dwVID, pkObj));
	m_map_pkObjByID.insert(std::make_pair(pTable->dwID, pkObj));

	pkLand->InsertObject(pkObj);

	if (!isBoot)
		pkObj->Show(pTable->lMapIndex, pTable->x, pTable->y);
	else
	{
		pkObj->SetMapIndex(pTable->lMapIndex);
		pkObj->SetXYZ(pTable->x, pTable->y, 0);
	}

	// BUILDING_NPC
	if (!isBoot)
	{ 
		if (pkProto->dwNPCVnum)
			pkObj->RegenNPC();

		pkObj->ApplySpecialEffect();
	}
	// END_OF_BUILDING_NPC

	return true;
}

void CManager::FinalizeBoot()
{
	itertype(m_map_pkObjByID) it = m_map_pkObjByID.begin();

	while (it != m_map_pkObjByID.end())
	{
		LPOBJECT pkObj = (it++)->second;

		pkObj->Show(pkObj->GetMapIndex(), pkObj->GetX(), pkObj->GetY());
		// BUILDING_NPC
		pkObj->RegenNPC();
		pkObj->ApplySpecialEffect();
		// END_OF_BUILDING_NPC
	}

	// BUILDING_NPC
	SPDLOG_DEBUG("FinalizeBoot");
	// END_OF_BUILDING_NPC

	itertype(m_map_pkLand) it2 = m_map_pkLand.begin();

	while (it2 != m_map_pkLand.end())
	{
		CLand * pkLand = (it2++)->second;

		const TLand & r = pkLand->GetData();

		// LAND_MASTER_LOG	
		SPDLOG_DEBUG("LandMaster map_index={} pos=({}, {})", r.lMapIndex, r.x, r.y);
		// END_OF_LAND_MASTER_LOG

		if (r.dwGuildID != 0)
			continue;

		if (!map_allow_find(r.lMapIndex))
			continue;

		const TMapRegion * region = SECTREE_MANAGER::instance().GetMapRegion(r.lMapIndex);
		if (!region)
			continue;

		CHARACTER_MANAGER::instance().SpawnMob(20040, r.lMapIndex, region->sx + r.x + (r.width / 2), region->sy + r.y + (r.height / 2), 0);
	}
}

void CManager::DeleteObject(DWORD dwID) // from DB
{
	SPDLOG_DEBUG("OBJ_DEL: {}", dwID);

	itertype(m_map_pkObjByID) it = m_map_pkObjByID.find(dwID);

	if (it == m_map_pkObjByID.end())
		return;

	it->second->GetLand()->DeleteObject(dwID);
}

LPOBJECT CManager::FindObjectByVID(DWORD dwVID)
{
	itertype(m_map_pkObjByVID) it = m_map_pkObjByVID.find(dwVID);

	if (it == m_map_pkObjByVID.end())
		return NULL;

	return it->second;
}

void CManager::UnregisterObject(LPOBJECT pkObj)
{
	m_map_pkObjByID.erase(pkObj->GetID());
	m_map_pkObjByVID.erase(pkObj->GetVID());
}

void CManager::SendLandList(LPDESC d, int lMapIndex)
{
	TLandPacketElement e;

	TEMP_BUFFER buf;

	WORD wCount = 0;

	itertype(m_map_pkLand) it = m_map_pkLand.begin();

	while (it != m_map_pkLand.end())
	{
		CLand * pkLand = (it++)->second;
		const TLand & r = pkLand->GetData();

		if (r.lMapIndex != lMapIndex)
			continue;

		//
		LPCHARACTER ch  = d->GetCharacter();
		if (ch)
		{
			CGuild *guild = CGuildManager::instance().FindGuild(r.dwGuildID);
			ch->SendGuildName(guild);
		}
		//

		e.dwID = r.dwID;
		e.x = r.x;
		e.y = r.y;
		e.width = r.width;
		e.height = r.height;
		e.dwGuildID = r.dwGuildID;

		buf.write(&e, sizeof(TLandPacketElement));
		++wCount;
	}

	SPDLOG_DEBUG("SendLandList map {} count {} elem_size: {}", lMapIndex, wCount, buf.size());

	if (wCount != 0)
	{
		TPacketGCLandList p;

		p.header = HEADER_GC_LAND_LIST;
		p.size = sizeof(TPacketGCLandList) + buf.size();

        d->RawPacket(&p, sizeof(TPacketGCLandList));
		d->Packet(buf.read_peek(), buf.size());
	}
}

// LAND_CLEAR
void CManager::ClearLand(DWORD dwLandID)
{
	CLand* pLand = FindLand(dwLandID);

	if ( pLand == NULL )
	{
		SPDLOG_WARN("LAND_CLEAR: there is no LAND id like {}", dwLandID);
		return;
	}

	pLand->ClearLand();

	SPDLOG_DEBUG("LAND_CLEAR: request Land Clear. LandID: {}", pLand->GetID());
}

void CManager::ClearLandByGuildID(DWORD dwGuildID)
{
	CLand* pLand = FindLandByGuild(dwGuildID);

	if ( pLand == NULL )
	{
		SPDLOG_WARN("LAND_CLEAR: there is no GUILD id like {}", dwGuildID);
		return;
	}

	pLand->ClearLand();

	SPDLOG_DEBUG("LAND_CLEAR: request Land Clear. LandID: {}", pLand->GetID());
}

void CLand::ClearLand()
{
	itertype(m_map_pkObject) iter = m_map_pkObject.begin();

	while ( iter != m_map_pkObject.end() )
	{
		RequestDeleteObject(iter->second->GetID());
		iter++;
	}

	SetOwner(0);

	const TLand & r = GetData();
	const TMapRegion * region = SECTREE_MANAGER::instance().GetMapRegion(r.lMapIndex);

	CHARACTER_MANAGER::instance().SpawnMob(20040, r.lMapIndex, region->sx + r.x + (r.width / 2), region->sy + r.y + (r.height / 2), 0);
}
// END_LAND_CLEAR

// BUILD_WALL
void CLand::DrawWall(DWORD dwVnum, int nMapIndex, int& x, int& y, char length, float zRot)
{
	int rot = (int)zRot;
	rot = ((rot%360) / 90) * 90;

	int dx=0, dy=0;

	switch ( rot )
	{
		case 0 :
			dx = -500;
			dy = 0;
			break;

		case 90 :
			dx = 0;
			dy = 500;
			break;

		case 180 :
			dx = 500;
			dy = 0;
			break;

		case 270 :
			dx = 0;
			dy = -500;
			break;
	}

	for ( int i=0; i < length; i++ )
	{
		this->RequestCreateObject(dwVnum, nMapIndex, x, y, 0, 0, rot, false);
		x += dx;
		y += dy;
	}
}


bool CLand::RequestCreateWall(int nMapIndex, float rot)
{
	const bool 	WALL_ANOTHER_CHECKING_ENABLE = false;

	const TLand& land = GetData();

	int center_x = land.x + land.width  / 2;
	int center_y = land.y + land.height / 2;

	int wall_x = center_x;
	int wall_y = center_y;
	int wall_half_w = 1000;
	int wall_half_h = 1362;

	if (rot == 0.0f) 		// 남쪽 문
	{
		int door_x = wall_x;
		int door_y = wall_y + wall_half_h;
		RequestCreateObject(WALL_DOOR_VNUM,	nMapIndex, wall_x, wall_y + wall_half_h, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_BACK_VNUM,	nMapIndex, wall_x, wall_y - wall_half_h, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_LEFT_VNUM,	nMapIndex, wall_x - wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_RIGHT_VNUM,	nMapIndex, wall_x + wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}	
	else if (rot == 180.0f)		// 북쪽 문
	{
		int door_x = wall_x;
		int door_y = wall_y - wall_half_h;
		RequestCreateObject(WALL_DOOR_VNUM,	nMapIndex, wall_x, wall_y - wall_half_h, door_x, door_y, 180.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_BACK_VNUM,	nMapIndex, wall_x, wall_y + wall_half_h, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_LEFT_VNUM,	nMapIndex, wall_x - wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_RIGHT_VNUM,	nMapIndex, wall_x + wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}
	else if (rot == 90.0f)		// 동쪽 문 
	{
		int door_x = wall_x + wall_half_h;
		int door_y = wall_y;
		RequestCreateObject(WALL_DOOR_VNUM,	nMapIndex, wall_x + wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_BACK_VNUM,	nMapIndex, wall_x - wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_LEFT_VNUM,	nMapIndex, wall_x, wall_y - wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_RIGHT_VNUM,	nMapIndex, wall_x, wall_y + wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}
	else if (rot == 270.0f)		// 서쪽 문 
	{
		int door_x = wall_x - wall_half_h;
		int door_y = wall_y;
		RequestCreateObject(WALL_DOOR_VNUM,	nMapIndex, wall_x - wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_BACK_VNUM,	nMapIndex, wall_x + wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_LEFT_VNUM,	nMapIndex, wall_x, wall_y - wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_RIGHT_VNUM,	nMapIndex, wall_x, wall_y + wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}

	if (test_server)
	{
		RequestCreateObject(FLAG_VNUM, nMapIndex, land.x + 50, 			land.y + 50, 0, 0, 0.0, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(FLAG_VNUM, nMapIndex, land.x + land.width - 50,	land.y + 50, 0, 0, 90.0, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(FLAG_VNUM, nMapIndex, land.x + land.width - 50,	land.y + land.height - 50, 0, 0, 180.0, WALL_ANOTHER_CHECKING_ENABLE); 
		RequestCreateObject(FLAG_VNUM, nMapIndex, land.x + 50, 			land.y + land.height - 50, 0, 0, 270.0, WALL_ANOTHER_CHECKING_ENABLE);
	}
	return true;
}

void CLand::RequestDeleteWall()
{
	itertype(m_map_pkObject) iter = m_map_pkObject.begin();

	while (iter != m_map_pkObject.end())
	{
		unsigned id   = iter->second->GetID();
		unsigned vnum = iter->second->GetVnum();

		switch (vnum)
		{
			case WALL_DOOR_VNUM:
			case WALL_BACK_VNUM:
			case WALL_LEFT_VNUM:
			case WALL_RIGHT_VNUM:
				RequestDeleteObject(id);
				break;
		}


		if (test_server)
		{
			if (FLAG_VNUM == vnum)
				RequestDeleteObject(id);

		}

		iter++;
	}
}

bool CLand::RequestCreateWallBlocks(DWORD dwVnum, int nMapIndex, char wallSize, bool doorEast, bool doorWest, bool doorSouth, bool doorNorth)
{
	const TLand & r = GetData();

	int startX = r.x + (r.width  / 2) - (1300 + wallSize*500);
	int startY = r.y + (r.height / 2) + (1300 + wallSize*500);

	DWORD corner = dwVnum - 4;
	DWORD wall   = dwVnum - 3;
	DWORD door   = dwVnum - 1;

	bool checkAnother = false;
	int* ptr = NULL;
	int delta = 1;
	int rot = 270;

	bool doorOpen[4];
	doorOpen[0] = doorWest;
	doorOpen[1] = doorNorth;
	doorOpen[2] = doorEast;
	doorOpen[3] = doorSouth;

	if ( wallSize > 3 ) wallSize = 3;
	else if ( wallSize < 0 ) wallSize = 0;

	for ( int i=0; i < 4; i++, rot -= 90 )
	{
		switch ( i )
		{
			case 0 :
				delta = -1;
				ptr = &startY;
				break;
			case 1 :
				delta = 1;
				ptr = &startX;
				break;
			case 2 :
				ptr = &startY;
				delta = 1;
				break;
			case 3 :
				ptr = &startX;
				delta = -1;
				break;
		}

		this->RequestCreateObject(corner, nMapIndex, startX, startY, 0, 0, rot, checkAnother);

		*ptr = *ptr + ( 700 * delta );

		if ( doorOpen[i] )
		{
			this->DrawWall(wall, nMapIndex, startX, startY, wallSize, rot);

			*ptr = *ptr + ( 700 * delta );

			this->RequestCreateObject(door, nMapIndex, startX, startY, 0, 0, rot, checkAnother);

			*ptr = *ptr + ( 1300 * delta );

			this->DrawWall(wall, nMapIndex, startX, startY, wallSize, rot);
		}
		else
		{
			this->DrawWall(wall, nMapIndex, startX, startY, wallSize*2 + 4, rot);
		}

		*ptr = *ptr + ( 100 * delta );
	}

	return true;
}

void CLand::RequestDeleteWallBlocks(DWORD dwID)
{
	itertype(m_map_pkObject) iter = m_map_pkObject.begin();

	DWORD corner = dwID - 4;
	DWORD wall = dwID - 3;
	DWORD door = dwID - 1;
	DWORD dwVnum = 0;

	while ( iter != m_map_pkObject.end() )
	{
		dwVnum = iter->second->GetVnum();

		if ( dwVnum == corner || dwVnum == wall || dwVnum == door )
		{
			RequestDeleteObject(iter->second->GetID());
		}
		iter++;
	}
}
// END_BUILD_WALL

