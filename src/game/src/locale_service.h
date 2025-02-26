#pragma once

extern std::string g_stLanguage;

bool LocaleService_Init(const std::string& language);
void LocaleService_LoadLocaleStringFile();
void LocaleService_LoadEmpireTextConvertTables();
void LocaleService_TransferDefaultSetting();
const std::string& LocaleService_GetBasePath();
const std::string& LocaleService_GetMapPath();
const std::string& LocaleService_GetQuestPath();

struct LanguageSettings
{
	std::string mysqlCharsetName;
	bool useLocaleString = true;
};
