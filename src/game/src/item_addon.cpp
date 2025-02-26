#include "stdafx.h"
#include "constants.h"
#include "utils.h"
#include "item.h"
#include "item_addon.h"

CItemAddonManager::CItemAddonManager()
{
}

CItemAddonManager::~CItemAddonManager()
{
}

void CItemAddonManager::ApplyAddonTo(int iAddonType, LPITEM pItem)
{
	if (!pItem)
	{
		SPDLOG_ERROR("ITEM pointer null");
		return;
	}

	// TODO 일단 하드코딩으로 평타 스킬 수치 변경만 경우만 적용받게한다.

	int iSkillBonus = std::clamp((int) (Random::get<std::normal_distribution<>>(0, 5) + 0.5f), -30, 30);
	int iNormalHitBonus = 0;
	if (abs(iSkillBonus) <= 20)
		iNormalHitBonus = -2 * iSkillBonus + abs(Random::get(-8, 8) + Random::get(-8, 8)) + Random::get(1, 4);
	else
		iNormalHitBonus = -2 * iSkillBonus + Random::get(1, 5);

	pItem->RemoveAttributeType(APPLY_SKILL_DAMAGE_BONUS);
	pItem->RemoveAttributeType(APPLY_NORMAL_HIT_DAMAGE_BONUS);
	pItem->AddAttribute(APPLY_NORMAL_HIT_DAMAGE_BONUS, iNormalHitBonus);
	pItem->AddAttribute(APPLY_SKILL_DAMAGE_BONUS, iSkillBonus);
}
