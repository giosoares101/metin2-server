//#define __FISHING_MAIN__
#include "stdafx.h"
#include "constants.h"
#include "fishing.h"
#include "locale_service.h"

#ifndef __FISHING_MAIN__
#include "item_manager.h"

#include "config.h"
#include "packet.h"

#include "sectree_manager.h"
#include "char.h"
#include "char_manager.h"

#include "log.h"

#include "questmanager.h"
#include "buffer_manager.h"
#include "desc_client.h"
#include "locale_service.h"

#include "affect.h"
#include "unique_item.h"
#endif

namespace fishing
{
	enum
	{
		MAX_FISH = 37,
		NUM_USE_RESULT_COUNT = 10, // 1 : DEAD 2 : BONE 3 ~ 12 : rest
		FISH_BONE_VNUM = 27799,
		SHELLFISH_VNUM = 27987,
		EARTHWORM_VNUM = 27801,
		WATER_STONE_VNUM_BEGIN = 28030,
		WATER_STONE_VNUM_END = 28043,
		FISH_NAME_MAX_LEN = 64,
		MAX_PROB = 4,
	};

	enum
	{
		USED_NONE,
		USED_SHELLFISH,
		USED_WATER_STONE,
		USED_TREASURE_MAP,
		USED_EARTHWARM,
		MAX_USED_FISH
	};

	enum
	{
		FISHING_TIME_NORMAL,
		FISHING_TIME_SLOW,
		FISHING_TIME_QUICK,
		FISHING_TIME_ALL,
		FISHING_TIME_EASY,

		FISHING_TIME_COUNT,

		MAX_FISHING_TIME_COUNT = 31,
	};

	enum
	{
		FISHING_LIMIT_NONE,
		FISHING_LIMIT_APPEAR,
	};

	int aFishingTime[FISHING_TIME_COUNT][MAX_FISHING_TIME_COUNT] =
	{
		{   0,   0,   0,   0,   0,   2,   4,   8,  12,  16,  20,  22,  25,  30,  50,  80,  50,  30,  25,  22,  20,  16,  12,   8,   4,   2,   0,   0,   0,   0,   0 },
		{   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   4,   8,  12,  16,  20,  22,  25,  30,  50,  80,  50,  30,  25,  22,  20 },
		{  20,  22,  25,  30,  50,  80,  50,  30,  25,  22,  20,  16,  12,   8,   4,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
		{ 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 },
		{  20,  20,  20,  20,  20,  22,  24,  28,  32,  36,  40,  42,  45,  50,  70, 100,  70,  50,  45,  42,  40,  36,  32,  28,  24,  22,  20,  20,  20,  20,  20 },
	};

	struct SFishInfo
	{
		char name[FISH_NAME_MAX_LEN];

		DWORD vnum;
		DWORD dead_vnum;
		DWORD grill_vnum;

		int prob[MAX_PROB];
		int difficulty;

		int time_type;
		int length_range[3]; // MIN MAX EXTRA_MAX : 99% MIN~MAX, 1% MAX~EXTRA_MAX

		int used_table[NUM_USE_RESULT_COUNT];
		// 6000 2000 1000 500 300 100 50 30 10 5 4 1
	};

	bool operator < ( const SFishInfo& lhs, const SFishInfo& rhs )
	{
		return lhs.vnum < rhs.vnum;
	}

	int g_prob_sum[MAX_PROB];
	int g_prob_accumulate[MAX_PROB][MAX_FISH];

	SFishInfo fish_info[MAX_FISH] = { { "\0", }, };
	/*
	   {
	   { "\xB2\xCE",		00000, 00000, 00000, {  750, 1750, 2750 },   10, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL, { 0,           }, 
	   {0, } },
	   { "\xB1\xDD\xB9\xDD\xC1\xF6",	50002, 00000, 00000, {   50,   50,    0 },  200, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL, { 0,           }, 
	   {0, } },
	   { "\xC7\xC7\xB6\xF3\xB9\xCC",	27802, 00000, 00000, { 2100, 1500,   50 },   10, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_EASY,   {500, 550, 600}, 
	   {0, } },
	   { "\xBA\xD8\xBE\xEE",	27803, 27833, 27863, { 2100, 1500,  100 },   13, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_EASY,   {1000,2500,2800},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xBD\xEE\xB0\xA1\xB8\xAE",	27804, 27834, 27864, { 1100, 1300,  150 },   16, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL, {2000,3500,3800},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xBF\xF9\xC3\xB4\xBA\xD8\xBE\xEE",	27805, 27835, 27865, { 1100, 1100,  200 },   20, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_SLOW, {3030,3500,4300},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xC0\xD7\xBE\xEE",	27806, 27836, 27866, { 1100,  500,  300 },   26, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL, {4000,6000,10000},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xBF\xAC\xBE\xEE",	27807, 27837, 27867, { 1100,  450,  400 },   33, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{6000,8000,10000},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xC7\xE2\xBE\xEE",	27808, 27838, 27868, {  200,  400,  500 },   42, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{1500,3000,3800},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xBC\xDB\xBE\xEE",	27809, 27839, 27869, {  200,  300,  700 },   54, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{5000,7000,8000},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xB9\xCE\xB9\xB0\xC0\xE5\xBE\xEE",	27810, 27840, 27870, {    0,  270, 1000 },   70, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{4000,5000,6000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xB9\xAB\xC1\xF6\xB0\xB3\xBC\xDB\xBE\xEE",	27811, 27841, 27871, {    0,  200, 1000 },   91, FISHING_LIMIT_APPEAR,	{  0,   0,   0}, FISHING_TIME_NORMAL,{5000,7000,8000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xB0\xAD\xBC\xDB\xBE\xEE",	27812, 27842, 27872, {    0,  160, 1000 },  118, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_QUICK,{4000,6000,7000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xB7\xAF\xB5\xE5",	27813, 27843, 27873, {    0,  130,  700 },  153, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{4000,6000,10000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xC6\xDB\xC4\xA1",	27814, 27844, 27874, {    0,  100,  400 },  198, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{3000,4000,5000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xC5\xD9\xC4\xA1",	27815, 27845, 27875, {    0,   50,  300 },  257, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{3500,5500,8000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xB8\xDE\xB1\xE2",	27816, 27846, 27876, {    0,   30,  100 },  334, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{3000,5000,10000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xB9\xCC\xB2\xD9\xB6\xF3\xC1\xF6",	27817, 27847, 27877, {    0,   10,   64 },  434, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_QUICK,{1800,2200,3000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xB9\xE9\xB7\xC3",	27818, 27848, 27878, {    0,    0,   15 },  564, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{5000,8000,10000},
	   {USED_SHELLFISH, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xC0\xBA\xBE\xEE",	27819, 27849, 27879, {    0,    0,    9 },  733, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{1500,3000,3800},
	   {USED_SHELLFISH, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xBA\xF9\xBE\xEE",	27820, 27850, 27880, {    0,    0,    6 },  952, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_QUICK,{1500,3000,3800},
	   {USED_SHELLFISH, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xBD\xAC\xB8\xAE",	27821, 27851, 27881, {    0,    0,    3 }, 1237, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{1000,1500,2000},
	   {USED_SHELLFISH, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xBA\xF1\xB4\xC3\xC0\xD7\xBE\xEE",	27822, 27852, 27882, {    0,    0,    2 }, 1608, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_SLOW,{4000,6000,10000},
	   {USED_SHELLFISH, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xC8\xB2\xB1\xDD\xBA\xD8\xBE\xEE",	27823, 27853, 27883, {    0,    0,    1 }, 2090, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_SLOW,{1000,3000,3500},
	   {USED_SHELLFISH, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "\xC5\xBB\xBB\xF6\xBE\xE0",     70201, 00000, 00000, { 5,    5,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "\xBF\xB0\xBB\xF6\xBE\xE0(\xC8\xF2\xBB\xF6)",  70202, 00000, 00000, { 15,  15,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "\xBF\xB0\xBB\xF6\xBE\xE0(\xB1\xDD\xBB\xF6)",  70203, 00000, 00000, { 15,  15,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "\xBF\xB0\xBB\xF6\xBE\xE0(\xBB\xA1\xB0\xA3\xBB\xF6)",70204, 00000, 00000, { 15,  15,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "\xBF\xB0\xBB\xF6\xBE\xE0(\xB0\xA5\xBB\xF6)",  70205, 00000, 00000, { 15,  15,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "\xBF\xB0\xBB\xF6\xBE\xE0(\xB0\xCB\xC0\xBA\xBB\xF6)",70206, 00000, 00000, { 15,  15,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "\xC0\xBA\xB5\xD0\xC0\xDA\xC0\xC7 \xB8\xC1\xC5\xE4", 70048, 00000, 00000, {  8,   8,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "\xC7\xE0\xBF\xEE\xC0\xC7 \xB9\xDD\xC1\xF6",   70049, 00000, 00000, {  8,   8,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "\xBC\xB1\xBF\xD5\xC0\xC7 \xC1\xF5\xC7\xA5",   70050, 00000, 00000, {  8,   8,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "\xBC\xB1\xBF\xD5\xC0\xC7 \xC0\xE5\xB0\xA9",   70051, 00000, 00000, {  8,   8,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "\xB1\xDD\xB5\xA2\xBE\xEE\xB8\xAE",	   80008, 00000, 00000, { 20,  20,  0 },  250, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_SLOW,    {0,           },
	   {0, } },
	{ "\xC0\xBA\xBF\xAD\xBC\xE8",	   50009, 00000, 00000, {300, 300, 0, },   70, FISHING_LIMIT_NONE,    { 0, 0, 0}, FISHING_TIME_NORMAL, {0,	}, {0, } },

	{ "\xB1\xDD\xBF\xAD\xBC\xE8",	   50008, 00000, 00000, {110, 110, 0, },  100, FISHING_LIMIT_NONE,    { 0, 0, 0}, FISHING_TIME_NORMAL, {0,	}, {0, } },
};
	*/
void Initialize()
{
	SFishInfo fish_info_bak[MAX_FISH];
	memcpy(fish_info_bak, fish_info, sizeof(fish_info));

	memset(fish_info, 0, sizeof(fish_info));


	// LOCALE_SERVICE
	const int FILE_NAME_LEN = 256;
	char szFishingFileName[FILE_NAME_LEN+1];
	snprintf(szFishingFileName, sizeof(szFishingFileName),
			"%s/fishing.txt", LocaleService_GetBasePath().c_str());
	FILE * fp = fopen(szFishingFileName, "r");
	// END_OF_LOCALE_SERVICE

	if (*fish_info_bak[0].name)
		SendLog("Reloading fish table.");

	if (!fp)
	{
		SendLog("error! cannot open fishing.txt");

		// 백업에 이름이 있으면 리스토어 한다.
		if (*fish_info_bak[0].name)
		{
			memcpy(fish_info, fish_info_bak, sizeof(fish_info));
			SendLog("  restoring to backup");
		}
		return;
	}

	memset(fish_info, 0, sizeof(fish_info));

	char buf[512];
	int idx = 0;

	while (fgets(buf, 512, fp))
	{
		if (*buf == '#')
			continue;

		char * p = strrchr(buf, '\n');
		*p = '\0';

		const char * start = buf;
		const char * tab = strchr(start, '\t');

		if (!tab)
		{
			printf("Tab error on line: %s\n", buf);
			SendLog("error! parsing fishing.txt");

			if (*fish_info_bak[0].name)
			{
				memcpy(fish_info, fish_info_bak, sizeof(fish_info));
				SendLog("  restoring to backup");
			}
			break;
		}

		char szCol[256], szCol2[256];
		int iColCount = 0;

		do
		{
			strlcpy(szCol2, start, std::min<size_t>(sizeof(szCol2), (tab - start) + 1));
			szCol2[tab-start] = '\0';

			trim_and_lower(szCol2, szCol, sizeof(szCol));

			if (!*szCol || *szCol == '\t')
				iColCount++;
			else 
			{
				switch (iColCount++)
				{
					case 0: strlcpy(fish_info[idx].name, szCol, sizeof(fish_info[idx].name)); break;
					case 1: str_to_number(fish_info[idx].vnum, szCol); break;
					case 2: str_to_number(fish_info[idx].dead_vnum, szCol); break;
					case 3: str_to_number(fish_info[idx].grill_vnum, szCol); break;
					case 4: str_to_number(fish_info[idx].prob[0], szCol); break;
					case 5: str_to_number(fish_info[idx].prob[1], szCol); break;
					case 6: str_to_number(fish_info[idx].prob[2], szCol); break;
					case 7: str_to_number(fish_info[idx].prob[3], szCol); break;
					case 8: str_to_number(fish_info[idx].difficulty, szCol); break;
					case 9: str_to_number(fish_info[idx].time_type, szCol); break;
					case 10: str_to_number(fish_info[idx].length_range[0], szCol); break;
					case 11: str_to_number(fish_info[idx].length_range[1], szCol); break;
					case 12: str_to_number(fish_info[idx].length_range[2], szCol); break;
					case 13: // 0
					case 14: // 1
					case 15: // 2
					case 16: // 3
					case 17: // 4
					case 18: // 5
					case 19: // 6
					case 20: // 7
					case 21: // 8
					case 22: // 9
							 str_to_number(fish_info[idx].used_table[iColCount-1-12], szCol);
							 break;
				}
			}

			start = tab + 1;
			tab = strchr(start, '\t');
		} while (tab);

		idx++;

		if (idx == MAX_FISH)
			break;
	}

	fclose(fp);

	for (int i = 0; i < MAX_FISH; ++i)
	{
        SPDLOG_TRACE("FISH: {:24} vnum {:5} prob {:4} {:4} {:4} {:4} len {} {} {}",
				fish_info[i].name,
				fish_info[i].vnum,
				fish_info[i].prob[0],
				fish_info[i].prob[1],
				fish_info[i].prob[2],
				fish_info[i].prob[3],
				fish_info[i].length_range[0],
				fish_info[i].length_range[1],
				fish_info[i].length_range[2]);
	}

	// 확률 계산
	for (int j = 0; j < MAX_PROB; ++j)
	{
		g_prob_accumulate[j][0] = fish_info[0].prob[j];

		for (int i = 1; i < MAX_FISH; ++i)
			g_prob_accumulate[j][i] = fish_info[i].prob[j] + g_prob_accumulate[j][i - 1];

		g_prob_sum[j] = g_prob_accumulate[j][MAX_FISH - 1];
		SPDLOG_DEBUG("FISH: prob table {} {}", j, g_prob_sum[j]);
	}
}

int DetermineFishByProbIndex(int prob_idx)
{
	int rv = Random::get(1, g_prob_sum[prob_idx]);
	int * p = std::lower_bound(g_prob_accumulate[prob_idx], g_prob_accumulate[prob_idx]+ MAX_FISH, rv);
	int fish_idx = p - g_prob_accumulate[prob_idx];
	return fish_idx;
}

int GetProbIndexByMapIndex(int index)
{
	if (index > 60)
		return -1;

	switch (index)
	{
		case 1:
		case 21:
		case 41:
			return 0;

		case 3:
		case 23:
		case 43:
			return 1;
	}

	return -1;
}

#ifndef __FISHING_MAIN__
int DetermineFish(LPCHARACTER ch)
{
	int map_idx = ch->GetMapIndex();
	int prob_idx = GetProbIndexByMapIndex(map_idx);

	if (prob_idx < 0) 
		return 0;

	// ADD_PREMIUM
	if (ch->GetPremiumRemainSeconds(PREMIUM_FISH_MIND) > 0 ||
			ch->IsEquipUniqueGroup(UNIQUE_GROUP_FISH_MIND)) // 월간어심 착용시 고급 물고기 확률 상승
	{
		if (quest::CQuestManager::instance().GetEventFlag("manwoo") != 0)
			prob_idx = 3;
		else
			prob_idx = 2;
	}
	// END_OF_ADD_PREMIUM

	int adjust = 0;
	if (quest::CQuestManager::instance().GetEventFlag("fish_miss_pct") != 0)
	{
		int fish_pct_value = std::clamp(quest::CQuestManager::instance().GetEventFlag("fish_miss_pct"), 0, 200);
		adjust = (100-fish_pct_value) * fish_info[0].prob[prob_idx] / 100;
	}

	int rv = Random::get(adjust + 1, g_prob_sum[prob_idx]);

	int * p = std::lower_bound(g_prob_accumulate[prob_idx], g_prob_accumulate[prob_idx] + MAX_FISH, rv);
	int fish_idx = p - g_prob_accumulate[prob_idx];

	DWORD vnum = fish_info[fish_idx].vnum;

	if (vnum == 50008 || vnum == 50009 || vnum == 80008) 
		return 0;

	return (fish_idx);
}

void FishingReact(LPCHARACTER ch)
{
	TPacketGCFishing p;
	p.header = HEADER_GC_FISHING;
	p.subheader = FISHING_SUBHEADER_GC_REACT;
	p.info = ch->GetVID();
	ch->PacketAround(&p, sizeof(p));
}

void FishingSuccess(LPCHARACTER ch)
{
	TPacketGCFishing p;
	p.header = HEADER_GC_FISHING;
	p.subheader = FISHING_SUBHEADER_GC_SUCCESS;
	p.info = ch->GetVID();
	ch->PacketAround(&p, sizeof(p));
}

void FishingFail(LPCHARACTER ch)
{
	TPacketGCFishing p;
	p.header = HEADER_GC_FISHING;
	p.subheader = FISHING_SUBHEADER_GC_FAIL;
	p.info = ch->GetVID();
	ch->PacketAround(&p, sizeof(p));
}

void FishingPractice(LPCHARACTER ch)
{
	LPITEM rod = ch->GetWear(WEAR_WEAPON);
	if (rod && rod->GetType() == ITEM_ROD)
	{
		// 최대 수련도가 아닌 경우 낚시대 수련
		if ( rod->GetRefinedVnum()>0 && rod->GetSocket(0) < rod->GetValue(2) && Random::get(1,rod->GetValue(1))==1 )
		{
			rod->SetSocket(0, rod->GetSocket(0) + 1);
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Your fishing points have increased! (%d/%d)"),rod->GetSocket(0), rod->GetValue(2));
			if (rod->GetSocket(0) == rod->GetValue(2))
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have reached the maximum number of fishing points."));
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Go to the Fisherman and get your Fishing Pole upgraded!"));
			}
		}
	}
	// 미끼를 뺀다
	rod->SetSocket(2, 0);
}

bool PredictFish(LPCHARACTER ch)
{
	// ADD_PREMIUM
	// 어심환
	if (ch->FindAffect(AFFECT_FISH_MIND_PILL) || 
			ch->GetPremiumRemainSeconds(PREMIUM_FISH_MIND) > 0 ||
			ch->IsEquipUniqueGroup(UNIQUE_GROUP_FISH_MIND))
		return true;
	// END_OF_ADD_PREMIUM

	return false;
}

EVENTFUNC(fishing_event)
{
	fishing_event_info * info = dynamic_cast<fishing_event_info *>( event->info );

	if ( info == NULL )
	{
		SPDLOG_ERROR("fishing_event> <Factor> Null pointer" );
		return 0;
	}

	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(info->pid);

	if (!ch)
		return 0;

	LPITEM rod = ch->GetWear(WEAR_WEAPON);

	if (!(rod && rod->GetType() == ITEM_ROD))
	{
		ch->m_pkFishingEvent = NULL;
		return 0;
	}

	switch (info->step)
	{
		case 0:	// 흔들리기 또는 떡밥만 날아감
			++info->step;

			//info->ch->Motion(MOTION_FISHING_SIGN);
			info->hang_time = get_dword_time();
			info->fish_id = DetermineFish(ch);
			FishingReact(ch);

			if (PredictFish(ch))
			{
				TPacketGCFishing p;
				p.header	= HEADER_GC_FISHING;
				p.subheader	= FISHING_SUBHEADER_GC_FISH;
				p.info	= fish_info[info->fish_id].vnum;
				ch->GetDesc()->Packet(&p, sizeof(TPacketGCFishing));
			}
			return (PASSES_PER_SEC(6));

		default:
			++info->step;

			if (info->step > 5)
				info->step = 5;

			ch->m_pkFishingEvent = NULL;
			FishingFail(ch);
			rod->SetSocket(2, 0);
			return 0;
	}
}

LPEVENT CreateFishingEvent(LPCHARACTER ch)
{
	fishing_event_info* info = AllocEventInfo<fishing_event_info>();
	info->pid	= ch->GetPlayerID();
	info->step	= 0;
	info->hang_time	= 0;

	int time = Random::get(10, 40);

	TPacketGCFishing p;
	p.header	= HEADER_GC_FISHING;
	p.subheader	= FISHING_SUBHEADER_GC_START;
	p.info		= ch->GetVID();
	p.dir		= (BYTE)(ch->GetRotation()/5);
	ch->PacketAround(&p, sizeof(TPacketGCFishing));

	return event_create(fishing_event, info, PASSES_PER_SEC(time));
}

int GetFishingLevel(LPCHARACTER ch)
{
	LPITEM rod = ch->GetWear(WEAR_WEAPON);

	if (!rod || rod->GetType()!= ITEM_ROD)
		return 0;

	return rod->GetSocket(2) + rod->GetValue(0);
}

int Compute(DWORD fish_id, DWORD ms, DWORD* item, int level)
{
	if (fish_id == 0)
		return -2;

	if (fish_id >= MAX_FISH)
	{
		SPDLOG_ERROR("Wrong FISH ID : {}", fish_id);
		return -2; 
	}

	if (ms > 6000)
		return -1;

	int time_step = std::clamp<int>(((ms + 99) / 200), 0, MAX_FISHING_TIME_COUNT - 1);

	if (Random::get(1, 100) <= aFishingTime[fish_info[fish_id].time_type][time_step])
	{
		if (Random::get(1, fish_info[fish_id].difficulty) <= level)
		{
			*item = fish_info[fish_id].vnum;
			return 0;
		}
		return -3;
	}

	return -1;
}

int GetFishLength(int fish_id)
{
	if (Random::get(0,99))
	{
		// 99% : normal size
		return (int)(fish_info[fish_id].length_range[0] + 
				(fish_info[fish_id].length_range[1] - fish_info[fish_id].length_range[0])
				* (Random::get(0,2000)+Random::get(0,2000)+Random::get(0,2000)+Random::get(0,2000)+Random::get(0,2000))/10000);
	}
	else
	{
		// 1% : extra LARGE size
		return (int)(fish_info[fish_id].length_range[1] + 
				(fish_info[fish_id].length_range[2] - fish_info[fish_id].length_range[1])
				* 2 * asin(Random::get(0,10000)/10000.) / M_PI);
	}
}

void Take(fishing_event_info* info, LPCHARACTER ch)
{
	if (info->step == 1)	// 고기가 걸린 상태면..
	{
		int ms = (int) ((get_dword_time() - info->hang_time));
		DWORD item_vnum = 0;
		int ret = Compute(info->fish_id, ms, &item_vnum, GetFishingLevel(ch));

		//if (test_server)
		//ch->ChatPacket(CHAT_TYPE_INFO, "%d ms", ms);

		switch (ret)
		{
			case -2: // 잡히지 않은 경우
			case -3: // 난이도 때문에 실패
			case -1: // 시간 확률 때문에 실패
				//ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You lost your bait to the fish."));
				{
					int map_idx = ch->GetMapIndex();
					int prob_idx = GetProbIndexByMapIndex(map_idx);

					LogManager::instance().FishLog(
							ch->GetPlayerID(),
							prob_idx,
							info->fish_id,
							GetFishingLevel(ch),
							ms);
				}
				FishingFail(ch);
				break;

			case 0:
				//ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have caught a fish! (%s)"), fish_info[info->fish_id].name);
				if (item_vnum)
				{
					FishingSuccess(ch);

					TPacketGCFishing p;
					p.header = HEADER_GC_FISHING;
					p.subheader = FISHING_SUBHEADER_GC_FISH;
					p.info = item_vnum;
					ch->GetDesc()->Packet(&p, sizeof(TPacketGCFishing));

					LPITEM item = ch->AutoGiveItem(item_vnum, 1, -1, false);
					if (item)
					{
						item->SetSocket(0,GetFishLength(info->fish_id));
						if (test_server)
						{
							ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The length of the captured fish is %.2fcm."), item->GetSocket(0)/100.f);
						}
					}

					int map_idx = ch->GetMapIndex();
					int prob_idx = GetProbIndexByMapIndex(map_idx);

					LogManager::instance().FishLog(
							ch->GetPlayerID(),
							prob_idx,
							info->fish_id,
							GetFishingLevel(ch),
							ms,
							true,
							item ? item->GetSocket(0) : 0);

				}
				else
				{
					int map_idx = ch->GetMapIndex();
					int prob_idx = GetProbIndexByMapIndex(map_idx);

					LogManager::instance().FishLog(
							ch->GetPlayerID(),
							prob_idx,
							info->fish_id,
							GetFishingLevel(ch),
							ms);
					FishingFail(ch);
				}
				break;
		}
	}
	else if (info->step > 1)
	{
		int map_idx = ch->GetMapIndex();
		int prob_idx = GetProbIndexByMapIndex(map_idx);

		LogManager::instance().FishLog(
				ch->GetPlayerID(),
				prob_idx,
				info->fish_id,
				GetFishingLevel(ch),
				7000);
		//ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You lost your bait to the fish."));
		FishingFail(ch);
	}
	else
	{
		TPacketGCFishing p;
		p.header = HEADER_GC_FISHING;
		p.subheader = FISHING_SUBHEADER_GC_STOP;
		p.info = ch->GetVID();
		ch->PacketAround(&p, sizeof(p));
	}

	if (info->step)
	{
		FishingPractice(ch);
	}
	//Motion(MOTION_FISHING_PULL);
}

void Simulation(int level, int count, int prob_idx, LPCHARACTER ch)
{
	std::map<std::string, int> fished;
	int total_count = 0;

	for (int i = 0; i < count; ++i)
	{
		int fish_id = DetermineFishByProbIndex(prob_idx);
		DWORD item = 0;
		Compute(fish_id, (Random::get(2000, 4000) + Random::get(2000,4000)) / 2, &item, level);

		if (item)
		{
			fished[fish_info[fish_id].name]++;
			total_count ++;
		}
	}

	for (std::map<std::string,int>::iterator it = fished.begin(); it != fished.end(); ++it)
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s: %d"), it->first.c_str(), it->second);

	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have caught %d of %d ."), fished.size(), total_count);
}

void UseFish(LPCHARACTER ch, LPITEM item)
{
	int idx = item->GetVnum() - fish_info[2].vnum+2;

	// 피라미 사용불가, 살아있는게 아닌건 사용불가

	if (idx<=1 || idx >= MAX_FISH)
		return;

	int r = Random::get(1, 10000);

	item->SetCount(item->GetCount()-1);

	if (r >= 4001)
	{
		// 죽은 물고기
		ch->AutoGiveItem(fish_info[idx].dead_vnum);
	}
	else if (r >= 2001)
	{
		// 생선뼈
		ch->AutoGiveItem(FISH_BONE_VNUM);
	}
	else
	{
		// 1000 500 300 100 50 30 10 5 4 1
		static int s_acc_prob[NUM_USE_RESULT_COUNT] = { 1000, 1500, 1800, 1900, 1950, 1980, 1990, 1995, 1999, 2000 };
		int u_index = std::lower_bound(s_acc_prob, s_acc_prob + NUM_USE_RESULT_COUNT, r) - s_acc_prob;

		switch (fish_info[idx].used_table[u_index])
		{
			case USED_TREASURE_MAP:	// 3
			case USED_NONE:		// 0
			case USED_WATER_STONE:	// 2
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The fish vanished in the depths of the water."));
				break;

			case USED_SHELLFISH:	// 1
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There is a Clam inside the Fish."));
				ch->AutoGiveItem(SHELLFISH_VNUM);
				break;

			case USED_EARTHWARM:	// 4
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There is a Worm inside the Fish."));
				ch->AutoGiveItem(EARTHWORM_VNUM);
				break;

			default:
				ch->AutoGiveItem(fish_info[idx].used_table[u_index]);
				break;
		}
	}
}

void Grill(LPCHARACTER ch, LPITEM item)
{
	/*if (item->GetVnum() < fish_info[3].vnum)
	  return;
	  int idx = item->GetVnum()-fish_info[3].vnum+3;
	  if (idx>=MAX_FISH)
	  idx = item->GetVnum()-fish_info[3].dead_vnum+3;
	  if (idx>=MAX_FISH)
	  return;*/
	int idx = -1;
	DWORD vnum = item->GetVnum();
	if (vnum >= 27803 && vnum <= 27830)
		idx = vnum - 27800;
	if (vnum >= 27833 && vnum <= 27860)
		idx = vnum - 27830;
	if (idx == -1)
		return;

	int count = item->GetCount();

	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You are roasting %s over the fire."), item->GetName());
	item->SetCount(0);
	ch->AutoGiveItem(fish_info[idx].grill_vnum, count);
}

bool RefinableRod(LPITEM rod)
{
	if (rod->GetType() != ITEM_ROD)
		return false;

	if (rod->IsEquipped())
		return false;

	return (rod->GetSocket(0) == rod->GetValue(2));
}

int RealRefineRod(LPCHARACTER ch, LPITEM item)
{
	if (!ch || !item)
		return 2;

	// REFINE_ROD_HACK_BUG_FIX
	if (!RefinableRod(item))
	{
		SPDLOG_ERROR("REFINE_ROD_HACK pid({}) item({}:{})", ch->GetPlayerID(), item->GetName(), item->GetID());

		LogManager::instance().RefineLog(ch->GetPlayerID(), item->GetName(), item->GetID(), -1, 1, "ROD_HACK");

		return 2;
	}
	// END_OF_REFINE_ROD_HACK_BUG_FIX

	LPITEM rod = item;	

	int iAdv = rod->GetValue(0) / 10;

	if (Random::get(1,100) <= rod->GetValue(3))
	{
		LogManager::instance().RefineLog(ch->GetPlayerID(), rod->GetName(), rod->GetID(), iAdv, 1, "ROD");

		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(rod->GetRefinedVnum(), 1);

		if (pkNewItem)
		{
			BYTE bCell = rod->GetCell();
			// 낚시대 개량 성공
			ITEM_MANAGER::instance().RemoveItem(rod, "REMOVE (REFINE FISH_ROD)");
			pkNewItem->AddToCharacter(ch, TItemPos (INVENTORY, bCell)); 
			LogManager::instance().ItemLog(ch, pkNewItem, "REFINE FISH_ROD SUCCESS", pkNewItem->GetName());
			return 1;
		}

		// 낚시대 개량 실패
		return 2;
	}
	else
	{
		LogManager::instance().RefineLog(ch->GetPlayerID(), rod->GetName(), rod->GetID(), iAdv, 0, "ROD");

		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(rod->GetValue(4), 1);
		if (pkNewItem)
		{
			BYTE bCell = rod->GetCell();
			// 낚시대 개량에 성공
			ITEM_MANAGER::instance().RemoveItem(rod, "REMOVE (REFINE FISH_ROD)");
			pkNewItem->AddToCharacter(ch, TItemPos(INVENTORY, bCell)); 
			LogManager::instance().ItemLog(ch, pkNewItem, "REFINE FISH_ROD FAIL", pkNewItem->GetName());
			return 0;
		}

		// 낚시대 개량 실패
		return 2;
	}
}
#endif
}

#ifdef __FISHING_MAIN__
int main(int argc, char **argv)
{
	//srandom(time(0) + getpid());
	srandomdev();
	/*
	   struct SFishInfo
	   {
	   const char* name;

	   DWORD vnum;
	   DWORD dead_vnum;
	   DWORD grill_vnum;

	   int prob[3];
	   int difficulty;

	   int limit_type;
	   int limits[3];

	   int time_type;
	   int length_range[3]; // MIN MAX EXTRA_MAX : 99% MIN~MAX, 1% MAX~EXTRA_MAX

	   int used_table[NUM_USE_RESULT_COUNT];
	// 6000 2000 1000 500 300 100 50 30 10 5 4 1
	};
	 */
	using namespace fishing;

	Initialize();

	for (int i = 0; i < MAX_FISH; ++i)
	{
		printf("%s\t%u\t%u\t%u\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d",
				fish_info[i].name,
				fish_info[i].vnum,
				fish_info[i].dead_vnum,
				fish_info[i].grill_vnum,
				fish_info[i].prob[0], 
				fish_info[i].prob[1], 
				fish_info[i].prob[2],
				fish_info[i].difficulty,
				fish_info[i].time_type,
				fish_info[i].length_range[0],
				fish_info[i].length_range[1],
				fish_info[i].length_range[2]);

		for (int j = 0; j < NUM_USE_RESULT_COUNT; ++j)
			printf("\t%d", fish_info[i].used_table[j]);

		puts("");
	}

	return 1;
}

#endif
