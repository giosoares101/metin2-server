#include "stdafx.h"
#include "locale_service.h"
#include "constants.h"
#include "banword.h"
#include "utils.h"
#include "mob_manager.h"
#include "empire_text_convert.h"
#include "config.h"
#include "skill_power.h"

using namespace std;

string g_stLanguage;

string g_stLocale = "euckr";
string g_stLocaleFilename;

BYTE PK_PROTECT_LEVEL = 15;

int (*check_name) (const char * str) = NULL;
int (*is_twobyte) (const char * str) = NULL;

int is_twobyte_gb2312(const char * str)
{
	if (!str || !*str)
		return 0;

	BYTE b1 = str[0];
	BYTE b2 = str[1];

	if (!(b1 & 0x80))
		return 0;

	if (b1 < 0xb0 || b1 > 0xf7 || b2 < 0xa1 || b2 > 0xfe)
		return 0;

	return 1;
}

int check_name_independent(const char * str)
{
	if (CBanwordManager::instance().CheckString(str, strlen(str)))
		return 0;

	// 몬스터 이름으로는 만들 수 없다.
	char szTmp[256];
	str_lower(str, szTmp, sizeof(szTmp));

	if (CMobManager::instance().Get(szTmp, false))
		return 0;

	return 1;
}

int check_name_alphabet(const char * str)
{
	const char*	tmp;

	if (!str || !*str)
		return 0;

	if (strlen(str) < 2)
		return 0;

	for (tmp = str; *tmp; ++tmp)
	{
		// 알파벳과 수자만 허용
		if (isdigit(*tmp) || isalpha(*tmp))
			continue;
		else
			return 0;
	}

	return check_name_independent(str);
}

void LocaleService_LoadLocaleStringFile()
{
	if (g_stLocaleFilename.empty())
		return;

	if (g_bAuthServer)
		return;

	SPDLOG_INFO("LocaleService {}", g_stLocaleFilename);

	locale_init(g_stLocaleFilename.c_str());
}

void LocaleService_LoadEmpireTextConvertTables()
{
	char szFileName[256];

	for (int iEmpire = 1; iEmpire <= 3; ++iEmpire)
	{
		snprintf(szFileName, sizeof(szFileName), "%s/lang%d.cvt", LocaleService_GetBasePath().c_str(), iEmpire);
		SPDLOG_INFO("Load {}", szFileName);

		LoadEmpireTextConvertTable(iEmpire, szFileName);
	}
}

bool LocaleService_Init(const std::string& language)
{
	map<string, LanguageSettings> localeConfiguration = {
		{"en", {"latin1", false}},
		{"ae", {"cp1256"}},
		{"cz", {"latin2"}},
		{"de", {"latin1"}},
		{"dk", {"latin1"}},
		{"es", {"latin1"}},
		{"fr", {"latin1"}},
		{"gr", {"greek"}},
		{"hu", {"latin2"}},
		{"it", {"latin1"}},
		{"kr", {"euckr"}},
		{"nl", {"latin1"}},
		{"pl", {"latin2"}},
		{"pt", {"latin1"}},
		{"ro", {"latin2"}},
		{"ru", {"cp1251"}},
		{"tr", {"latin5"}},
	};

	if (!g_stLanguage.empty())
	{
		SPDLOG_ERROR("Locale was already initialized!");
		return false;
	}

	if (localeConfiguration.find(language) == localeConfiguration.end())
	{
		SPDLOG_ERROR("An unsupported language configuration was requested: {}", language);
		return false;
	}

	g_stLanguage = language;

	g_stLocale = localeConfiguration[language].mysqlCharsetName;
	g_iUseLocale = localeConfiguration[language].useLocaleString;

	if (g_iUseLocale)
		g_stLocaleFilename = g_stBasePath + "/locale_string_" + language + ".txt";

	SPDLOG_INFO("Setting language \"{}\"", g_stLanguage.c_str());
	
	return true;
}

void LocaleService_TransferDefaultSetting()
{
	if (!check_name)
		check_name = check_name_alphabet;

	if (!is_twobyte)
		is_twobyte = is_twobyte_gb2312;

	if (!CTableBySkill::instance().Check())
		exit(EXIT_FAILURE);
}

const std::string& LocaleService_GetBasePath()
{
	return g_stBasePath;
}

const std::string& LocaleService_GetMapPath()
{
	return g_stMapPath;
}

const std::string& LocaleService_GetQuestPath()
{
	return g_stQuestDir;
}
