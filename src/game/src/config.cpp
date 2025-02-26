#include "stdafx.h"
#include <sstream>
#ifndef __WIN32__
#include <ifaddrs.h>
#endif

#include "constants.h"
#include "utils.h"
#include "log.h"
#include "desc.h"
#include "desc_manager.h"
#include "item_manager.h"
#include "p2p.h"
#include "char.h"
#include "war_map.h"
#include "locale_service.h"
#include "config.h"
#include "db.h"
#include "skill_power.h"

using std::string;

// Networking
string	g_szPublicBindIP = "0.0.0.0";
string	g_szPublicIP;
WORD	mother_port = 50080;

string	g_szInternalBindIP = "0.0.0.0";
string	g_szInternalIP;
WORD	p2p_port = 50900;

char	db_addr[ADDRESS_MAX_LEN + 1];
WORD	db_port = 0;
// End of networking

BYTE	g_bChannel = 0;
int		passes_per_sec = 25;
int		save_event_second_cycle = passes_per_sec * 120;	// 3분
int		ping_event_second_cycle = passes_per_sec * 60;
bool	g_bNoMoreClient = false;
bool	g_bNoRegen = false;

// TRAFFIC_PROFILER
bool		g_bTrafficProfileOn = false;
DWORD		g_dwTrafficProfileFlushCycle = 3600;
// END_OF_TRAFFIC_PROFILER

int			test_server = 0;
int			speed_server = 0;
#ifdef __AUCTION__
int			auction_server = 0;
#endif
bool		distribution_test_server = false;
bool		china_event_server = false;
bool		guild_mark_server = true;
BYTE		guild_mark_min_level = 3;
bool		no_wander = false;
int		g_iUserLimit = 32768;

bool		g_bSkillDisable = false;
int			g_iFullUserCount = 1200;
int			g_iBusyUserCount = 650;
//Canada
//int			g_iFullUserCount = 600;
//int			g_iBusyUserCount = 350;
//Brazil
//int			g_iFullUserCount = 650;
//int			g_iBusyUserCount = 450;
bool		g_bEmpireWhisper = true;
BYTE		g_bAuthServer = false;

bool		g_bCheckClientVersion = true;
string	g_stClientVersion = "1215955205";

string	g_stAuthMasterIP;
WORD		g_wAuthMasterPort = 0;

static std::set<DWORD> s_set_dwFileCRC;
static std::set<DWORD> s_set_dwProcessCRC;

string g_stHostname = "";
string g_table_postfix = "";

string g_stBasePath = "data";
string g_stMapPath = "data/map";
string g_stQuestDir = "data/quest";
string g_stDefaultQuestObjectDir = "data/quest/object";
std::set<string> g_setQuestObjectDir;

std::vector<std::string>	g_stAdminPageIP;
std::string	g_stAdminPagePassword = "SHOWMETHEMONEY";

string g_stBlockDate = "30000705";

extern string g_stLocale;

int SPEEDHACK_LIMIT_COUNT   = 50;
int SPEEDHACK_LIMIT_BONUS   = 80;
int g_iSyncHackLimitCount = 20; // 10 -> 20 2013 09 11 CYH

//시야 = VIEW_RANGE + VIEW_BONUS_RANGE
//VIEW_BONUSE_RANGE : 클라이언트와 시야 처리에서너무 딱 떨어질경우 문제가 발생할수있어 500CM의 여분을 항상준다.
int VIEW_RANGE = 5000;
int VIEW_BONUS_RANGE = 500;

int g_server_id = 0;
string g_strWebAppURL = "";
string g_strWebAppKey = "";

unsigned int g_uiSpamBlockDuration = 60 * 15; // 기본 15분
unsigned int g_uiSpamBlockScore = 100; // 기본 100점
unsigned int g_uiSpamReloadCycle = 60 * 10; // 기본 10분

bool		g_bCheckMultiHack = true;

int			g_iSpamBlockMaxLevel = 10;

void		LoadStateUserCount();
void		LoadValidCRCList();
bool		LoadClientVersion();
bool            g_protectNormalPlayer   = false;        // 범법자가 "평화모드" 인 일반유저를 공격하지 못함
bool            g_noticeBattleZone      = false;        // 중립지대에 입장하면 안내메세지를 알려줌

int gPlayerMaxLevel = 99;

bool g_BlockCharCreation = false;

bool is_string_true(const char * string)
{
	bool	result = 0;
	if (isdigit(*string))
	{
		str_to_number(result, string);
		return result > 0 ? true : false;
	}
	else if (LOWER(*string) == 't')
		return true;
	else
		return false;
}

static std::set<int> s_set_map_allows;

bool map_allow_find(int index)
{
	if (g_bAuthServer)
		return false;

	if (s_set_map_allows.find(index) == s_set_map_allows.end())
		return false;

	return true;
}

void map_allow_log()
{
	std::set<int>::iterator i;

	for (i = s_set_map_allows.begin(); i != s_set_map_allows.end(); ++i)
		SPDLOG_INFO("MAP_ALLOW: {}", *i);
}

void map_allow_add(int index)
{
	if (map_allow_find(index) == true)
	{
        SPDLOG_CRITICAL("!!! FATAL ERROR !!! multiple MAP_ALLOW setting!!");
		exit(EXIT_FAILURE);
	}

    if (s_set_map_allows.size() >= MAP_ALLOW_MAX_LEN)
    {
        SPDLOG_CRITICAL("Fatal error: maximum allowed maps reached!");
        exit(EXIT_FAILURE);
    }

	SPDLOG_INFO("MAP ALLOW {}", index);
	s_set_map_allows.insert(index);
}

void map_allow_copy(int * pl, int size)
{
	int iCount = 0;

	for (auto mapId: s_set_map_allows)
	{
        if (iCount >= size)
            break;

		pl[iCount++] = mapId;
	}
}

static void FN_add_adminpageIP(char *line)
{
	char	*last;
	const char *delim = " \t\r\n";
	char *v = strtok_r(line, delim, &last);

	while (v)
	{
		g_stAdminPageIP.push_back(v);
		v = strtok_r(NULL, delim, &last);
	}
}

static void FN_log_adminpage()
{
	itertype(g_stAdminPageIP) iter = g_stAdminPageIP.begin();

	while (iter != g_stAdminPageIP.end())
	{
		SPDLOG_TRACE("ADMIN_PAGE_IP = {}", (*iter).c_str());
		++iter;
	}

	SPDLOG_TRACE("ADMIN_PAGE_PASSWORD = {}", g_stAdminPagePassword.c_str());
}

/**
 * Checks if a given IPv4 address is in the Private Address Space as per RFC1918
 * @param address
 * @return True if address is private, false otherwise
 */
bool IsPrivateIP(const in_addr &address)
{
    const auto CLASS_A_PRIVATE = inet_addr("10.0.0.0");
    const auto CLASS_A_PRIVATE_NETMASK = inet_addr("255.0.0.0");
    if ((address.s_addr & CLASS_A_PRIVATE_NETMASK) == CLASS_A_PRIVATE)
        return true;

    const auto CLASS_B_PRIVATE = inet_addr("172.16.0.0");
    const auto CLASS_B_PRIVATE_NETMASK = inet_addr("255.240.0.0");
    if ((address.s_addr & CLASS_B_PRIVATE_NETMASK) == CLASS_B_PRIVATE)
        return true;

    const auto CLASS_C_PRIVATE = inet_addr("192.168.0.0");
    const auto CLASS_C_PRIVATE_NETMASK = inet_addr("255.255.0.0");
    if ((address.s_addr & CLASS_C_PRIVATE_NETMASK) == CLASS_C_PRIVATE)
        return true;

    return false;
}

/**
 * Retrieves the first private and public IP addresses and allocates them
 * to g_szInternalIP and g_szPublicIP respectively
 * @return True on success, false on system failure.
 */
bool GetIPInfo()
{
    ifaddrs* ifaddrp = nullptr;

	if (0 != getifaddrs(&ifaddrp))
		return false;

	for (ifaddrs* ifap = ifaddrp; ifap != nullptr; ifap = ifap->ifa_next)
	{
		struct sockaddr_in * sai = (struct sockaddr_in *) ifap->ifa_addr;

		if (!ifap->ifa_netmask ||  // ignore if no netmask
				sai->sin_addr.s_addr == 0 || // ignore if address is 0.0.0.0
				sai->sin_addr.s_addr == 16777343) // ignore if address is 127.0.0.1
			continue;

		if (IsPrivateIP(sai->sin_addr) && g_szInternalIP.empty())
		{
            char * ip = inet_ntoa(sai->sin_addr);
            g_szInternalIP = string(ip);
			SPDLOG_WARN("Internal IP automatically configured: {} interface {}", ip, ifap->ifa_name);
		}
		else if (g_szPublicIP.empty())
		{
            char * ip = inet_ntoa(sai->sin_addr);
            g_szPublicIP = string(ip);
            SPDLOG_WARN("Public IP automatically configured: {} interface {}", ip, ifap->ifa_name);
		}
	}

	freeifaddrs(ifaddrp);
    return true;
}

void config_init(const string& st_localeServiceName)
{
	FILE	*fp;

	char	buf[256];
	char	token_string[256];
	char	value_string[256];

	// LOCALE_SERVICE
	string	st_configFileName;

	st_configFileName.reserve(32);
	st_configFileName = "game.conf";

	if (!st_localeServiceName.empty())
	{
		st_configFileName += ".";
		st_configFileName += st_localeServiceName;
	}
	// END_OF_LOCALE_SERVICE

	if (!(fp = fopen(st_configFileName.c_str(), "r")))
	{
        SPDLOG_CRITICAL("Can not open [{}]", st_configFileName);
		exit(EXIT_FAILURE);
	}

	if (!GetIPInfo())
	{
        SPDLOG_CRITICAL("Failure in retrieving IP address information!");
		exit(EXIT_FAILURE);
	}

	char db_host[2][64], db_user[2][64], db_pwd[2][64], db_db[2][64];
	// ... 아... db_port는 이미 있는데... 네이밍 어찌해야함...
	int mysql_db_port[2];

	for (int n = 0; n < 2; ++n)
	{
		*db_host[n]	= '\0';
		*db_user[n] = '\0';
		*db_pwd[n]= '\0';
		*db_db[n]= '\0';
		mysql_db_port[n] = 0;
	}

	char log_host[64], log_user[64], log_pwd[64], log_db[64];
	int log_port = 0;

	*log_host = '\0';
	*log_user = '\0';
	*log_pwd = '\0';
	*log_db = '\0';


	// DB에서 로케일정보를 세팅하기위해서는 다른 세팅값보다 선행되어서
	// DB정보만 읽어와 로케일 세팅을 한후 다른 세팅을 적용시켜야한다.
	// 이유는 로케일관련된 초기화 루틴이 곳곳에 존재하기 때문.

	bool isCommonSQL = false;	
	bool isPlayerSQL = false;

	FILE* fpOnlyForDB;

	if (!(fpOnlyForDB = fopen(st_configFileName.c_str(), "r")))
	{
        SPDLOG_CRITICAL("Can not open [{}]", st_configFileName);
		exit(EXIT_FAILURE);
	}

	while (fgets(buf, 256, fpOnlyForDB))
	{
		parse_token(buf, token_string, value_string);

        // If the configuration string is empty, ignore this line
        if (strcmp(value_string, "") == 0)
            continue;

		TOKEN("BLOCK_LOGIN")
		{
			g_stBlockDate = value_string;
		}

		TOKEN("adminpage_ip")
		{
			FN_add_adminpageIP(value_string);
			//g_stAdminPageIP[0] = value_string;
		}

		TOKEN("adminpage_ip1")
		{
			FN_add_adminpageIP(value_string);
			//g_stAdminPageIP[0] = value_string;
		}

		TOKEN("adminpage_ip2")
		{
			FN_add_adminpageIP(value_string);
			//g_stAdminPageIP[1] = value_string;
		}

		TOKEN("adminpage_ip3")
		{
			FN_add_adminpageIP(value_string);
			//g_stAdminPageIP[2] = value_string;
		}

		TOKEN("adminpage_password")
		{
			g_stAdminPagePassword = value_string;
		}

		TOKEN("hostname")
		{
			g_stHostname = value_string;
			SPDLOG_INFO("HOSTNAME: {}", g_stHostname);
			continue;
		}

		TOKEN("channel")
		{
			str_to_number(g_bChannel, value_string);
			continue;
		}

		TOKEN("player_sql")
		{
			const char * line = two_arguments(value_string, db_host[0], sizeof(db_host[0]), db_user[0], sizeof(db_user[0]));
			line = two_arguments(line, db_pwd[0], sizeof(db_pwd[0]), db_db[0], sizeof(db_db[0]));

			if ('\0' != line[0])
			{
				char buf[256];
				one_argument(line, buf, sizeof(buf));
				str_to_number(mysql_db_port[0], buf);
			}

			if (!*db_host[0] || !*db_user[0] || !*db_pwd[0] || !*db_db[0])
			{
				SPDLOG_CRITICAL("PLAYER_SQL syntax: logsql <host user password db>");
				exit(EXIT_FAILURE);
			}

			char buf[1024];
			snprintf(buf, sizeof(buf), "PLAYER_SQL: %s %s %s %s %d", db_host[0], db_user[0], db_pwd[0], db_db[0], mysql_db_port[0]);
			isPlayerSQL = true;
			continue;
		}

		TOKEN("common_sql")
		{
			const char * line = two_arguments(value_string, db_host[1], sizeof(db_host[1]), db_user[1], sizeof(db_user[1]));
			line = two_arguments(line, db_pwd[1], sizeof(db_pwd[1]), db_db[1], sizeof(db_db[1]));

			if ('\0' != line[0])
			{
				char buf[256];
				one_argument(line, buf, sizeof(buf));
				str_to_number(mysql_db_port[1], buf);
			}

			if (!*db_host[1] || !*db_user[1] || !*db_pwd[1] || !*db_db[1])
			{
                SPDLOG_CRITICAL("COMMON_SQL syntax: logsql <host user password db>");
				exit(EXIT_FAILURE);
			}

			char buf[1024];
			snprintf(buf, sizeof(buf), "COMMON_SQL: %s %s %s %s %d", db_host[1], db_user[1], db_pwd[1], db_db[1], mysql_db_port[1]);
			isCommonSQL = true;
			continue;
		}

		TOKEN("log_sql")
		{
			const char * line = two_arguments(value_string, log_host, sizeof(log_host), log_user, sizeof(log_user));
			line = two_arguments(line, log_pwd, sizeof(log_pwd), log_db, sizeof(log_db));

			if ('\0' != line[0])
			{
				char buf[256];
				one_argument(line, buf, sizeof(buf));
				str_to_number(log_port, buf);
			}

			if (!*log_host || !*log_user || !*log_pwd || !*log_db)
			{
                SPDLOG_CRITICAL("LOG_SQL syntax: logsql <host user password db>");
				exit(EXIT_FAILURE);
			}

			char buf[1024];
			snprintf(buf, sizeof(buf), "LOG_SQL: %s %s %s %s %d", log_host, log_user, log_pwd, log_db, log_port);
			continue;
		}
    }

	//처리가 끝났으니 파일을 닫자.
	fclose(fpOnlyForDB);

	// CONFIG_SQL_INFO_ERROR
	if (!isCommonSQL)
	{
		puts("LOAD_COMMON_SQL_INFO_FAILURE:");
		puts("");
		puts("CONFIG:");
		puts("------------------------------------------------");
		puts("COMMON_SQL: HOST USER PASSWORD DATABASE");
		puts("");
		exit(EXIT_FAILURE);
	}

	if (!isPlayerSQL)
	{
		puts("LOAD_PLAYER_SQL_INFO_FAILURE:");
		puts("");
		puts("CONFIG:");
		puts("------------------------------------------------");
		puts("PLAYER_SQL: HOST USER PASSWORD DATABASE");
		puts("");
		exit(EXIT_FAILURE);
	}

	// Common DB 가 Locale 정보를 가지고 있기 때문에 가장 먼저 접속해야 한다.
	AccountDB::instance().Connect(db_host[1], mysql_db_port[1], db_user[1], db_pwd[1], db_db[1]);

	if (false == AccountDB::instance().IsConnected())
	{
		SPDLOG_CRITICAL("cannot start server while no common sql connected");
		exit(EXIT_FAILURE);
	}

	SPDLOG_INFO("CommonSQL connected");

	// Initialize locale settings
	{
		std::string szQuery = "SELECT mKey, mValue FROM locale WHERE mKey = 'LANGUAGE'";

		std::unique_ptr<SQLMsg> pMsg(AccountDB::instance().DirectQuery(szQuery.c_str()));

		if (pMsg->Get()->uiNumRows == 0)
		{
            SPDLOG_CRITICAL("COMMON_SQL: LANGUAGE setting was not found in database!");
			exit(EXIT_FAILURE);
		}

		MYSQL_ROW row; 

		while (NULL != (row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
		{
			// 로케일 세팅
			if (strcasecmp(row[0], "LANGUAGE") == 0)
			{
				if (LocaleService_Init(row[1]) == false)
				{
					SPDLOG_CRITICAL("COMMON_SQL: invalid locale key {}", row[1]);
					exit(EXIT_FAILURE);
				}
			}
		}
	}

	// 로케일 정보를 COMMON SQL에 세팅해준다.
	// 참고로 g_stLocale 정보는 LocaleService_Init() 내부에서 세팅된다.
	SPDLOG_INFO("Setting DB to locale {}", g_stLocale.c_str());

	AccountDB::instance().SetLocale(g_stLocale);

	AccountDB::instance().ConnectAsync(db_host[1], mysql_db_port[1], db_user[1], db_pwd[1], db_db[1], g_stLocale.c_str());

	// Player DB 접속
	DBManager::instance().Connect(db_host[0], mysql_db_port[0], db_user[0], db_pwd[0], db_db[0]);

	if (!DBManager::instance().IsConnected())
	{
		SPDLOG_CRITICAL("PlayerSQL.ConnectError");
		exit(EXIT_FAILURE);
	}

	SPDLOG_INFO("PlayerSQL connected");

	if (false == g_bAuthServer) // 인증 서버가 아닐 경우
	{
		// Log DB 접속
		LogManager::instance().Connect(log_host, log_port, log_user, log_pwd, log_db);

		if (!LogManager::instance().IsConnected())
		{
			SPDLOG_CRITICAL("LogSQL.ConnectError");
			exit(EXIT_FAILURE);
		}

		SPDLOG_INFO("LogSQL connected");

		LogManager::instance().BootLog(g_stHostname.c_str(), g_bChannel);
	}

	// SKILL_POWER_BY_LEVEL
	// 스트링 비교의 문제로 인해서 AccountDB::instance().SetLocale(g_stLocale) 후부터 한다.
	// 물론 국내는 별로 문제가 안된다(해외가 문제)
	{
		char szQuery[256];
		snprintf(szQuery, sizeof(szQuery), "SELECT mValue FROM locale WHERE mKey='SKILL_POWER_BY_LEVEL'");
		std::unique_ptr<SQLMsg> pMsg(AccountDB::instance().DirectQuery(szQuery));

		if (pMsg->Get()->uiNumRows == 0)
		{
			SPDLOG_CRITICAL("[SKILL_PERCENT] Query failed: {}", szQuery);
			exit(EXIT_FAILURE);
		}

		MYSQL_ROW row; 

		row = mysql_fetch_row(pMsg->Get()->pSQLResult);

		const char * p = row[0];
		int cnt = 0;
		char num[128];
		int aiBaseSkillPowerByLevelTable[SKILL_MAX_LEVEL+1];

		SPDLOG_INFO("SKILL_POWER_BY_LEVEL {}", p);
		while (*p != '\0' && cnt < (SKILL_MAX_LEVEL + 1))
		{
			p = one_argument(p, num, sizeof(num));
			aiBaseSkillPowerByLevelTable[cnt++] = atoi(num);

			if (*p == '\0')
			{
				if (cnt != (SKILL_MAX_LEVEL + 1))
				{
					SPDLOG_CRITICAL("[SKILL_PERCENT] locale table has not enough skill information! (count: {} query: {})", cnt, szQuery);
					exit(EXIT_FAILURE);
				}

				SPDLOG_INFO("SKILL_POWER_BY_LEVEL: Done! (count {})", cnt);
				break;
			}
		}

		// 종족별 스킬 세팅
		for (int job = 0; job < JOB_MAX_NUM * 2; ++job)
		{
			snprintf(szQuery, sizeof(szQuery), "SELECT mValue from locale where mKey='SKILL_POWER_BY_LEVEL_TYPE%d' ORDER BY CAST(mValue AS unsigned)", job);
			std::unique_ptr<SQLMsg> pMsg(AccountDB::instance().DirectQuery(szQuery));

			// 세팅이 안되어있으면 기본테이블을 사용한다.
			if (pMsg->Get()->uiNumRows == 0)
			{
				CTableBySkill::instance().SetSkillPowerByLevelFromType(job, aiBaseSkillPowerByLevelTable);
				continue;
			}

			row = mysql_fetch_row(pMsg->Get()->pSQLResult);
			cnt = 0;
			p = row[0];
			int aiSkillTable[SKILL_MAX_LEVEL + 1];

			SPDLOG_INFO("SKILL_POWER_BY_JOB {} {}", job, p);
			while (*p != '\0' && cnt < (SKILL_MAX_LEVEL + 1))
			{			
				p = one_argument(p, num, sizeof(num));
				aiSkillTable[cnt++] = atoi(num);

				if (*p == '\0')
				{
					if (cnt != (SKILL_MAX_LEVEL + 1))
					{
						SPDLOG_CRITICAL("[SKILL_PERCENT] locale table has not enough skill information! (count: {} query: {})", cnt, szQuery);
						exit(EXIT_FAILURE);
					}

					SPDLOG_INFO("SKILL_POWER_BY_JOB: Done! (job: {} count: {})", job, cnt);
					break;
				}
			}

			CTableBySkill::instance().SetSkillPowerByLevelFromType(job, aiSkillTable);
		}		
	}
	// END_SKILL_POWER_BY_LEVEL

	while (fgets(buf, 256, fp))
	{
		parse_token(buf, token_string, value_string);

        // If the configuration string is empty, ignore this line
        if (strcmp(value_string, "") == 0)
            continue;

		TOKEN("empire_whisper")
		{
			bool b_value = 0;
			str_to_number(b_value, value_string);
			g_bEmpireWhisper = !!b_value;
			continue;
		}

		TOKEN("mark_server")
		{
			guild_mark_server = is_string_true(value_string);
			continue;
		}

		TOKEN("mark_min_level")
		{
			str_to_number(guild_mark_min_level, value_string);
			guild_mark_min_level = std::clamp<BYTE>(guild_mark_min_level, 0, GUILD_MAX_LEVEL);
			continue;
		}

		TOKEN("log_level")
		{
			int i = 0;
			str_to_number(i, value_string);
			log_set_level(std::clamp(i, SPDLOG_LEVEL_TRACE, SPDLOG_LEVEL_OFF));
			continue;
		}

		TOKEN("passes_per_sec")
		{
			str_to_number(passes_per_sec, value_string);
			continue;
		}

        TOKEN("public_ip")
        {
            g_szPublicIP = string(value_string);
            SPDLOG_INFO("Setting public IP address to '{}'...", g_szPublicIP);
            continue;
        }

        TOKEN("public_bind_ip")
        {
            g_szPublicBindIP = string(value_string);
            continue;
        }

        TOKEN("port")
        {
            str_to_number(mother_port, value_string);
            continue;
        }

        TOKEN("internal_ip")
        {
            g_szInternalIP = string(value_string);
            SPDLOG_INFO("Setting internal IP address to '{}'...", g_szInternalIP);
            continue;
        }

        TOKEN("internal_bind_ip")
        {
            g_szInternalBindIP = string(value_string);
            continue;
        }

		TOKEN("p2p_port")
		{
			str_to_number(p2p_port, value_string);
			continue;
		}

		TOKEN("db_port")
		{
			str_to_number(db_port, value_string);
			continue;
		}

		TOKEN("db_addr")
		{
			strlcpy(db_addr, value_string, sizeof(db_addr));

			for (int n =0; n < ADDRESS_MAX_LEN; ++n)
			{
				if (db_addr[n] == ' ')
					db_addr[n] = '\0';
			}

			continue;
		}

		TOKEN("save_event_second_cycle")
		{
			int	cycle = 0;
			str_to_number(cycle, value_string);
			save_event_second_cycle = cycle * passes_per_sec;
			continue;
		}

		TOKEN("ping_event_second_cycle")
		{
			int	cycle = 0;
			str_to_number(cycle, value_string);
			ping_event_second_cycle = cycle * passes_per_sec;
			continue;
		}

		TOKEN("table_postfix")
		{
			g_table_postfix = value_string;
			continue;
		}

		TOKEN("test_server")
		{
			str_to_number(test_server, value_string);

			if (test_server) {
				printf("-----------------------------------------------\n");
				printf("TEST_SERVER\n");
				printf("-----------------------------------------------\n");
			}

			continue;
		}

		TOKEN("speed_server")
		{
			str_to_number(speed_server, value_string);

			if (speed_server) {
				printf("-----------------------------------------------\n");
				printf("SPEED_SERVER\n");
				printf("-----------------------------------------------\n");
			}

			continue;
		}
#ifdef __AUCTION__
		TOKEN("auction_server")
		{
			printf("-----------------------------------------------\n");
			printf("AUCTION_SERVER\n");
			printf("-----------------------------------------------\n");
			str_to_number(auction_server, value_string);
			continue;
		}
#endif
		TOKEN("distribution_test_server")
		{
			str_to_number(distribution_test_server, value_string);
			continue;
		}

		TOKEN("china_event_server")
		{
			str_to_number(china_event_server, value_string);
			continue;
		}

		TOKEN("shutdowned")
		{
			g_bNoMoreClient = true;
			continue;
		}

		TOKEN("no_regen")
		{
			g_bNoRegen = true;
			continue;
		}

		TOKEN("traffic_profile")
		{
			g_bTrafficProfileOn = true;
			continue;
		}


		TOKEN("map_allow")
		{
			char * p = value_string;
			string stNum;

			for (; *p; p++)
			{   
				if (isspace(*p))
				{
					if (stNum.length())
					{
						int	index = 0;
						str_to_number(index, stNum.c_str());
						map_allow_add(index);
						stNum.clear();
					}
				}
				else
					stNum += *p;
			}

			if (stNum.length())
			{
				int	index = 0;
				str_to_number(index, stNum.c_str());
				map_allow_add(index);
			}

			continue;
		}

		TOKEN("no_wander")
		{
			no_wander = true;
			continue;
		}

		TOKEN("user_limit")
		{
			str_to_number(g_iUserLimit, value_string);
			continue;
		}

		TOKEN("skill_disable")
		{
			str_to_number(g_bSkillDisable, value_string);
			continue;
		}

		TOKEN("auth_server")
		{
			char szIP[32];
			char szPort[32];

			two_arguments(value_string, szIP, sizeof(szIP), szPort, sizeof(szPort));

			// Skip if arguments are empty
			if (!*szIP)
				continue;

			if (!*szPort && strcasecmp(szIP, "master"))
			{
				SPDLOG_CRITICAL("AUTH_SERVER: syntax error: <ip|master> <port>");
				exit(EXIT_FAILURE);
			}

			g_bAuthServer = true;

			if (!strcasecmp(szIP, "master"))
				SPDLOG_INFO("AUTH_SERVER: I am the master");
			else
			{
				g_stAuthMasterIP = szIP;
				str_to_number(g_wAuthMasterPort, szPort);

				SPDLOG_INFO("AUTH_SERVER: master {} {}", g_stAuthMasterIP.c_str(), g_wAuthMasterPort);
			}
			continue;
		}

		TOKEN("quest_dir")
		{
			SPDLOG_INFO("QUEST_DIR SETTING : {}", value_string);
			g_stQuestDir = value_string;
		}

		TOKEN("quest_object_dir")
		{
			//g_stQuestObjectDir = value_string;
			std::istringstream is(value_string);
			SPDLOG_INFO("QUEST_OBJECT_DIR SETTING : {}", value_string);
			string dir;
			while (!is.eof())
			{
				is >> dir;
				if (is.fail())
					break;
				g_setQuestObjectDir.insert(dir);
				SPDLOG_INFO("QUEST_OBJECT_DIR INSERT : {}", dir .c_str());
			}
		}

		TOKEN("synchack_limit_count")
		{
			str_to_number(g_iSyncHackLimitCount, value_string);
		}

		TOKEN("speedhack_limit_count")
		{
			str_to_number(SPEEDHACK_LIMIT_COUNT, value_string);
		}

		TOKEN("speedhack_limit_bonus")
		{
			str_to_number(SPEEDHACK_LIMIT_BONUS, value_string);
		}

		TOKEN("server_id")
		{
			str_to_number(g_server_id, value_string);
		}

		TOKEN("web_app_url")
		{
			g_strWebAppURL = value_string;
		}

		TOKEN("web_app_key")
		{
			g_strWebAppKey = value_string;
		}

		TOKEN("view_range")
		{
			str_to_number(VIEW_RANGE, value_string);
		}

		TOKEN("spam_block_duration")
		{
			str_to_number(g_uiSpamBlockDuration, value_string);
		}

		TOKEN("spam_block_score")
		{
			str_to_number(g_uiSpamBlockScore, value_string);
			g_uiSpamBlockScore = std::max<int>(1, g_uiSpamBlockScore);
		}

		TOKEN("spam_block_reload_cycle")
		{
			str_to_number(g_uiSpamReloadCycle, value_string);
			g_uiSpamReloadCycle = std::max<int>(60, g_uiSpamReloadCycle); // 최소 1분
		}

		TOKEN("check_multihack")
		{
			str_to_number(g_bCheckMultiHack, value_string);
		}

		TOKEN("spam_block_max_level")
		{
			str_to_number(g_iSpamBlockMaxLevel, value_string);
		}
		TOKEN("protect_normal_player")
		{
			str_to_number(g_protectNormalPlayer, value_string);
		}
		TOKEN("notice_battle_zone")
		{
			str_to_number(g_noticeBattleZone, value_string);
		}

		TOKEN("pk_protect_level")
		{
		    str_to_number(PK_PROTECT_LEVEL, value_string);
		    SPDLOG_INFO("PK_PROTECT_LEVEL: {}", PK_PROTECT_LEVEL);
		}

		TOKEN("max_level")
		{
			str_to_number(gPlayerMaxLevel, value_string);

			gPlayerMaxLevel = std::clamp<int>(gPlayerMaxLevel, 1, PLAYER_MAX_LEVEL_CONST);

			SPDLOG_INFO("PLAYER_MAX_LEVEL: {}", gPlayerMaxLevel);
		}

		TOKEN("block_char_creation")
		{
			int tmp = 0;

			str_to_number(tmp, value_string);

			if (0 == tmp)
				g_BlockCharCreation = false;
			else
				g_BlockCharCreation = true;

			continue;
		}
	}

	if (g_setQuestObjectDir.empty())
		g_setQuestObjectDir.insert(g_stDefaultQuestObjectDir);

	if (0 == db_port)
	{
		SPDLOG_CRITICAL("DB_PORT not configured");
		exit(EXIT_FAILURE);
	}

	if (0 == g_bChannel)
	{
		SPDLOG_CRITICAL("CHANNEL not configured");
		exit(EXIT_FAILURE);
	}

	if (g_stHostname.empty())
	{
		SPDLOG_CRITICAL("HOSTNAME must be configured.");
		exit(EXIT_FAILURE);
	}

	// Validate web app configuration
	if (g_strWebAppURL.empty())
	{
		SPDLOG_CRITICAL("WEB_APP_URL must be configured.");
		exit(EXIT_FAILURE);
	}

	if (g_strWebAppKey.empty())
	{
		SPDLOG_CRITICAL("WEB_APP_KEY must be configured.");
		exit(EXIT_FAILURE);
	}

	// LOCALE_SERVICE 
	LocaleService_LoadLocaleStringFile();
	LocaleService_TransferDefaultSetting();
	LocaleService_LoadEmpireTextConvertTables();
	// END_OF_LOCALE_SERVICE

	fclose(fp);

	if ((fp = fopen("CMD", "r")))
	{
		while (fgets(buf, 256, fp))
		{
			char cmd[32], levelname[32];
			int level;

			two_arguments(buf, cmd, sizeof(cmd), levelname, sizeof(levelname));

			if (!*cmd || !*levelname)
			{
				SPDLOG_CRITICAL("CMD syntax error: <cmd> <DISABLE | LOW_WIZARD | WIZARD | HIGH_WIZARD | GOD>");
				exit(EXIT_FAILURE);
			}

			if (!strcasecmp(levelname, "LOW_WIZARD"))
				level = GM_LOW_WIZARD;
			else if (!strcasecmp(levelname, "WIZARD"))
				level = GM_WIZARD;
			else if (!strcasecmp(levelname, "HIGH_WIZARD"))
				level = GM_HIGH_WIZARD;
			else if (!strcasecmp(levelname, "GOD"))
				level = GM_GOD;
			else if (!strcasecmp(levelname, "IMPLEMENTOR"))
				level = GM_IMPLEMENTOR;
			else if (!strcasecmp(levelname, "DISABLE"))
				level = GM_IMPLEMENTOR + 1;
			else
			{
				SPDLOG_CRITICAL("CMD syntax error: <cmd> <DISABLE | LOW_WIZARD | WIZARD | HIGH_WIZARD | GOD>");
				exit(EXIT_FAILURE);
			}

			interpreter_set_privilege(cmd, level);
		}

		fclose(fp);
	}

	LoadValidCRCList();
	LoadStateUserCount();

	CWarMapManager::instance().LoadWarMapInfo(NULL);

	FN_log_adminpage();
}

const char* get_table_postfix()
{
	return g_table_postfix.c_str();
}

void LoadValidCRCList()
{
	s_set_dwProcessCRC.clear();
	s_set_dwFileCRC.clear();

	FILE * fp;
	char buf[256];

	if ((fp = fopen("CRC", "r")))
	{
		while (fgets(buf, 256, fp))
		{
			if (!*buf)
				continue;

			DWORD dwValidClientProcessCRC;
			DWORD dwValidClientFileCRC;

			sscanf(buf, " %u %u ", &dwValidClientProcessCRC, &dwValidClientFileCRC);

			s_set_dwProcessCRC.insert(dwValidClientProcessCRC);
			s_set_dwFileCRC.insert(dwValidClientFileCRC);

			SPDLOG_INFO("CLIENT_CRC: {} {}", dwValidClientProcessCRC, dwValidClientFileCRC);
		}

		fclose(fp);
	}
}

bool LoadClientVersion()
{
	FILE * fp = fopen("VERSION", "r");

	if (!fp)
		return false;

	char buf[256];
	fgets(buf, 256, fp);

	char * p = strchr(buf, '\n');
	if (p) *p = '\0';

	SPDLOG_INFO("VERSION: \"{}\"", buf);

	g_stClientVersion = buf;
	fclose(fp);
	return true;
}

void CheckClientVersion()
{
	g_bCheckClientVersion = true;

	const DESC_MANAGER::DESC_SET & set = DESC_MANAGER::instance().GetClientSet();
	DESC_MANAGER::DESC_SET::const_iterator it = set.begin();

	while (it != set.end())
	{
		LPDESC d = *(it++);

		if (!d->GetCharacter())
			continue;


		int version = atoi(g_stClientVersion.c_str());
		int date	= atoi(d->GetClientVersion() );

		//if (0 != g_stClientVersion.compare(d->GetClientVersion()) )
		if (version > date)
		{
			d->GetCharacter()->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("You do not have the correct client version. Please install the normal patch."));
			d->DelayedDisconnect(10);
		}
	}
}

void LoadStateUserCount()
{
	FILE * fp = fopen("state_user_count", "r");

	if (!fp)
		return;

	fscanf(fp, " %d %d ", &g_iFullUserCount, &g_iBusyUserCount);

	fclose(fp);
}

bool IsValidProcessCRC(DWORD dwCRC)
{
	return s_set_dwProcessCRC.find(dwCRC) != s_set_dwProcessCRC.end();
}

bool IsValidFileCRC(DWORD dwCRC)
{
	return s_set_dwFileCRC.find(dwCRC) != s_set_dwFileCRC.end();
}


