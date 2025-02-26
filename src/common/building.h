#ifndef __METIN_II_COMMON_BUILDING_H__
#define __METIN_II_COMMON_BUILDING_H__

namespace building 
{
	enum
	{
		OBJECT_MATERIAL_MAX_NUM = 5,
	};

	typedef struct SLand
	{
		DWORD	dwID;
		int	lMapIndex;
		int	x, y;
		int	width, height;
		DWORD	dwGuildID;
		BYTE	bGuildLevelLimit;
		DWORD	dwPrice;
	} TLand;

	typedef struct SObjectMaterial
	{
		DWORD	dwItemVnum;
		DWORD	dwCount;
	} TObjectMaterial;

	typedef struct SObjectProto
	{
		DWORD	dwVnum;
		DWORD	dwPrice;

		TObjectMaterial kMaterials[OBJECT_MATERIAL_MAX_NUM];

		DWORD	dwUpgradeVnum;
		DWORD	dwUpgradeLimitTime;
		int	lLife;
		int	lRegion[4];

		DWORD	dwNPCVnum;
		int	lNPCX;
		int	lNPCY;

		DWORD	dwGroupVnum; // 같은 그룹은 하나만 건설가능
		DWORD	dwDependOnGroupVnum; // 지어져 있어야하는 그룹
	} TObjectProto;

	typedef struct SObject
	{
		DWORD	dwID;
		DWORD	dwLandID;
		DWORD	dwVnum;
		int	lMapIndex;
		int	x, y;

		float	xRot;
		float	yRot;
		float	zRot;
		int	lLife;
	} TObject;
};

#endif
