#ifndef __INC_METIN_II_GAME_SECTREE_MANAGER_H__
#define __INC_METIN_II_GAME_SECTREE_MANAGER_H__

#include "sectree.h"


typedef struct SMapRegion
{
	int			index;
	int			sx, sy, ex, ey;
	PIXEL_POSITION	posSpawn;

	bool		bEmpireSpawnDifferent;
	PIXEL_POSITION	posEmpire[3];

	std::string		strMapName;
} TMapRegion;

struct TAreaInfo
{
	int sx, sy, ex, ey, dir;
	TAreaInfo(int sx, int sy, int ex, int ey, int dir)
		: sx(sx), sy(sy), ex(ex), ey(ey), dir(dir)
		{}
};

struct npc_info
{
	BYTE bType;
	const char* name;
	int x, y;
	npc_info(BYTE bType, const char* name, int x, int y)
		: bType(bType), name(name), x(x), y(y)
		{}
};

typedef std::map<std::string, TAreaInfo> TAreaMap;

typedef struct SSetting
{
	int			iIndex;
	int			iCellScale;
	int			iBaseX;
	int			iBaseY;
	int			iWidth;
	int			iHeight;

	PIXEL_POSITION	posSpawn;
} TMapSetting;

class SECTREE_MAP
{
	public:
		typedef std::map<DWORD, LPSECTREE> MapType;

		SECTREE_MAP();
		SECTREE_MAP(SECTREE_MAP & r);
		virtual ~SECTREE_MAP();

		bool Add(DWORD key, LPSECTREE sectree) {
			return map_.insert(MapType::value_type(key, sectree)).second;
		}

		LPSECTREE	Find(DWORD dwPackage);
		LPSECTREE	Find(DWORD x, DWORD y);
		void		Build();

		TMapSetting	m_setting;

		template< typename Func >
		void for_each( Func & rfunc )
		{
			// <Factor> Using snapshot copy to avoid side-effects
			FCollectEntity collector;
			std::map<DWORD, LPSECTREE>::iterator it = map_.begin();
			for ( ; it != map_.end(); ++it)
			{
				LPSECTREE sectree = it->second;
				sectree->for_each_entity(collector);
			}
			collector.ForEach(rfunc);
			/*
			std::map<DWORD,LPSECTREE>::iterator i = map_.begin();
			for (; i != map_.end(); ++i )
			{
				LPSECTREE pSec = i->second;
				pSec->for_each_entity( rfunc );
			}
			*/
		}

		void DumpAllToSysErr() {
			SECTREE_MAP::MapType::iterator i;
			for (i = map_.begin(); i != map_.end(); ++i)
			{
				SPDLOG_ERROR("SECTREE {}({}, {})", i->first, i->first & 0xffff, i->first >> 16);
			}
		}

	private:
		MapType map_;
};

enum EAttrRegionMode
{
	ATTR_REGION_MODE_SET,
	ATTR_REGION_MODE_REMOVE,
	ATTR_REGION_MODE_CHECK,
};

class SECTREE_MANAGER : public singleton<SECTREE_MANAGER>
{
	public:
		SECTREE_MANAGER();
		virtual ~SECTREE_MANAGER();

		LPSECTREE_MAP GetMap(int lMapIndex);
		LPSECTREE 	Get(DWORD dwIndex, DWORD package);
		LPSECTREE 	Get(DWORD dwIndex, DWORD x, DWORD y);

		template< typename Func >
		void for_each( int iMapIndex, Func & rfunc )
		{
			LPSECTREE_MAP pSecMap = SECTREE_MANAGER::instance().GetMap( iMapIndex );
			if ( pSecMap )
			{
				pSecMap->for_each( rfunc );
			}
		}
		
		int		LoadSettingFile(int lIndex, const char * c_pszSettingFileName, TMapSetting & r_setting);
		bool		LoadMapRegion(const char * c_pszFileName, TMapSetting & r_Setting, const char * c_pszMapName);
		int		Build(const char * c_pszListFileName, const char* c_pszBasePath);
		LPSECTREE_MAP BuildSectreeFromSetting(TMapSetting & r_setting);
		bool		LoadAttribute(LPSECTREE_MAP pkMapSectree, const char * c_pszFileName, TMapSetting & r_setting);
		void		LoadDungeon(int iIndex, const char * c_pszFileName);
		bool		GetValidLocation(int lMapIndex, int x, int y, int& r_lValidMapIndex, PIXEL_POSITION & r_pos, BYTE empire = 0);
		bool		GetSpawnPosition(int x, int y, PIXEL_POSITION & r_pos);
		bool		GetSpawnPositionByMapIndex(int lMapIndex, PIXEL_POSITION & r_pos);
		bool		GetRecallPositionByEmpire(int iMapIndex, BYTE bEmpire, PIXEL_POSITION & r_pos);

		const TMapRegion *	GetMapRegion(int lMapIndex);
		int			GetMapIndex(int x, int y);
		const TMapRegion *	FindRegionByPartialName(const char* szMapName);

		bool		GetMapBasePosition(int x, int y, PIXEL_POSITION & r_pos);
		bool		GetMapBasePositionByMapIndex(int lMapIndex, PIXEL_POSITION & r_pos);
		bool		GetMovablePosition(int lMapIndex, int x, int y, PIXEL_POSITION & pos);
		bool		IsMovablePosition(int lMapIndex, int x, int y);
		bool		GetCenterPositionOfMap(int lMapIndex, PIXEL_POSITION & r_pos);
		bool        GetRandomLocation(int lMapIndex, PIXEL_POSITION & r_pos, DWORD dwCurrentX = 0, DWORD dwCurrentY = 0, int iMaxDistance = 0);

		int		CreatePrivateMap(int lMapIndex);	// returns new private map index, returns 0 when fail
		void		DestroyPrivateMap(int lMapIndex);

		TAreaMap&	GetDungeonArea(int lMapIndex);
		void		SendNPCPosition(LPCHARACTER ch);
		void		InsertNPCPosition(int lMapIndex, BYTE bType, const char* szName, int x, int y);

		BYTE		GetEmpireFromMapIndex(int lMapIndex);

		void		PurgeMonstersInMap(int lMapIndex);
		void		PurgeStonesInMap(int lMapIndex);
		void		PurgeNPCsInMap(int lMapIndex);
		size_t		GetMonsterCountInMap(int lMapIndex);
		size_t		GetMonsterCountInMap(int lMpaIndex, DWORD dwVnum);

		/// 영역에 대해 Sectree 의 Attribute 에 대해 특정한 처리를 수행한다.
		/**
		 * @param [in]	lMapIndex 적용할 Map index
		 * @param [in]	lStartX 사각형 영역의 가장 왼쪽 좌표
		 * @param [in]	lStartY 사각형 영역의 가장 위쪽 좌표
		 * @param [in]	lEndX 사각형 영역의 가장 오른쪽 좌표
		 * @param [in]	lEndY 사각형 영역의 가장 아랫쪽 좌표
		 * @param [in]	lRotate 영역에 대해 회전할 각
		 * @param [in]	dwAttr 적용할 Attribute
		 * @param [in]	mode Attribute 에 대해 처리할 type
		 */
		bool		ForAttrRegion(int lMapIndex, int lStartX, int lStartY, int lEndX, int lEndY, int lRotate, DWORD dwAttr, EAttrRegionMode mode);

		bool		SaveAttributeToImage(int lMapIndex, const char * c_pszFileName, LPSECTREE_MAP pMapSrc = NULL);

	private:

		/// 직각의 사각형 영역에 대해 Sectree 의 Attribute 에 대해 특정한 처리를 수행한다.
		/**
		 * @param [in]	lMapIndex 적용할 Map index
		 * @param [in]	lCX 사각형 영역의 가장 왼쪽 Cell 의 좌표
		 * @param [in]	lCY 사각형 영역의 가장 위쪽 Cell 의 좌표
		 * @param [in]	lCW 사각형 영역의 Cell 단위 폭
		 * @param [in]	lCH 사각형 영역의 Cell 단위 높이
		 * @param [in]	lRotate 회전할 각(직각)
		 * @param [in]	dwAttr 적용할 Attribute
		 * @param [in]	mode Attribute 에 대해 처리할 type
		 */
		bool		ForAttrRegionRightAngle( int lMapIndex, int lCX, int lCY, int lCW, int lCH, int lRotate, DWORD dwAttr, EAttrRegionMode mode );

		/// 직각 이외의 사각형 영역에 대해 Sectree 의 Attribute 에 대해 특정한 처리를 수행한다.
		/**
		 * @param [in]	lMapIndex 적용할 Map index
		 * @param [in]	lCX 사각형 영역의 가장 왼쪽 Cell 의 좌표
		 * @param [in]	lCY 사각형 영역의 가장 위쪽 Cell 의 좌표
		 * @param [in]	lCW 사각형 영역의 Cell 단위 폭
		 * @param [in]	lCH 사각형 영역의 Cell 단위 높이
		 * @param [in]	lRotate 회전할 각(직각 이외의 각)
		 * @param [in]	dwAttr 적용할 Attribute
		 * @param [in]	mode Attribute 에 대해 처리할 type
		 */
		bool		ForAttrRegionFreeAngle( int lMapIndex, int lCX, int lCY, int lCW, int lCH, int lRotate, DWORD dwAttr, EAttrRegionMode mode );

		/// 한 Cell 의 Attribute 에 대해 특정한 처리를 수행한다.
		/**
		 * @param [in]	lMapIndex 적용할 Map index
		 * @param [in]	lCX 적용할 Cell 의 X 좌표
		 * @param [in]	lCY 적용할 Cell 의 Y 좌표
		 * @param [in]	dwAttr 적용할 Attribute
		 * @param [in]	mode Attribute 에 대해 처리할 type
		 */
		bool		ForAttrRegionCell( int lMapIndex, int lCX, int lCY, DWORD dwAttr, EAttrRegionMode mode );

		static WORD			current_sectree_version;
		std::map<DWORD, LPSECTREE_MAP>	m_map_pkSectree;
		std::map<int, TAreaMap>	m_map_pkArea;
		std::vector<TMapRegion>		m_vec_mapRegion;
		std::map<DWORD, std::vector<npc_info> > m_mapNPCPosition;

		// <Factor> Circular private map indexing
		typedef std::unordered_map<int, int> PrivateIndexMapType;
		PrivateIndexMapType next_private_index_map_;
};

#endif

