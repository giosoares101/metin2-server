#include "stdafx.h"
#include "../../libgame/include/grid.h"
#include "utils.h"
#include "desc.h"
#include "desc_client.h"
#include "char.h"
#include "item.h"
#include "item_manager.h"
#include "packet.h"
#include "log.h"
#include "db.h"
#include "locale_service.h"
#include <common/length.h>
#include "exchange.h"
#include "DragonSoul.h"

void exchange_packet(LPCHARACTER ch, BYTE sub_header, bool is_me, DWORD arg1, TItemPos arg2, DWORD arg3, void * pvData = NULL);

// 교환 패킷
void exchange_packet(LPCHARACTER ch, BYTE sub_header, bool is_me, DWORD arg1, TItemPos arg2, DWORD arg3, void * pvData)
{
	if (!ch->GetDesc())
		return;

	struct packet_exchange pack_exchg;

	pack_exchg.header 		= HEADER_GC_EXCHANGE;
	pack_exchg.sub_header 	= sub_header;
	pack_exchg.is_me		= is_me;
	pack_exchg.arg1		= arg1;
	pack_exchg.arg2		= arg2;
	pack_exchg.arg3		= arg3;

	if (sub_header == EXCHANGE_SUBHEADER_GC_ITEM_ADD && pvData)
	{
		memcpy(&pack_exchg.alSockets, ((LPITEM) pvData)->GetSockets(), sizeof(pack_exchg.alSockets));
		memcpy(&pack_exchg.aAttr, ((LPITEM) pvData)->GetAttributes(), sizeof(pack_exchg.aAttr));
	}
	else
	{
		memset(&pack_exchg.alSockets, 0, sizeof(pack_exchg.alSockets));
		memset(&pack_exchg.aAttr, 0, sizeof(pack_exchg.aAttr));
	}

	ch->GetDesc()->Packet(&pack_exchg, sizeof(pack_exchg));
}

// 교환을 시작
bool CHARACTER::ExchangeStart(LPCHARACTER victim)
{
	if (this == victim)	// 자기 자신과는 교환을 못한다.
		return false;

	if (IsObserverMode())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot trade while observing."));
		return false;
	}

	if (victim->IsNPC())
		return false;

	//PREVENT_TRADE_WINDOW
	if ( IsOpenSafebox() || GetShopOwner() || GetMyShop() || IsCubeOpen())
	{
		ChatPacket( CHAT_TYPE_INFO, LC_TEXT("If the window is open, you cannot trade with others." ) );
		return false;
	}

	if ( victim->IsOpenSafebox() || victim->GetShopOwner() || victim->GetMyShop() || victim->IsCubeOpen() )
	{
		ChatPacket( CHAT_TYPE_INFO, LC_TEXT("The other person is currently busy so you cannot trade right now." ) );
		return false;
	}
	//END_PREVENT_TRADE_WINDOW
	int iDist = DISTANCE_APPROX(GetX() - victim->GetX(), GetY() - victim->GetY());

	// 거리 체크
	if (iDist >= EXCHANGE_MAX_DISTANCE)
		return false;

	if (GetExchange())
		return false;

	if (victim->GetExchange())
	{
		exchange_packet(this, EXCHANGE_SUBHEADER_GC_ALREADY, 0, 0, NPOS, 0);
		return false;
	}

	if (victim->IsBlockMode(BLOCK_EXCHANGE))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The other person has cancelled the trade."));
		return false;
	}

	SetExchange(M2_NEW CExchange(this));
	victim->SetExchange(M2_NEW CExchange(victim));

	victim->GetExchange()->SetCompany(GetExchange());
	GetExchange()->SetCompany(victim->GetExchange());

	//
	SetExchangeTime();
	victim->SetExchangeTime();

	exchange_packet(victim, EXCHANGE_SUBHEADER_GC_START, 0, GetVID(), NPOS, 0);
	exchange_packet(this, EXCHANGE_SUBHEADER_GC_START, 0, victim->GetVID(), NPOS, 0);

	return true;
}

CExchange::CExchange(LPCHARACTER pOwner)
{
	m_pCompany = NULL;

	m_bAccept = false;

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		m_apItems[i] = NULL;
		m_aItemPos[i] = NPOS;
		m_abItemDisplayPos[i] = 0;
	}

	m_lGold = 0;

	m_pOwner = pOwner;
	pOwner->SetExchange(this);

	m_pGrid = M2_NEW CGrid(4,3);
}

CExchange::~CExchange()
{
	M2_DELETE(m_pGrid);
}

bool CExchange::AddItem(TItemPos item_pos, BYTE display_pos)
{
	assert(m_pOwner != NULL && GetCompany());

	if (!item_pos.IsValidItemPosition())
		return false;

	// 장비는 교환할 수 없음
	if (item_pos.IsEquipPosition())
		return false;

	LPITEM item;

	if (!(item = m_pOwner->GetItem(item_pos)))
		return false;

	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE))
	{
		m_pOwner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot trade this item."));
		return false;
	}

	if (true == item->isLocked())
	{
		return false;
	}

	// 이미 교환창에 추가된 아이템인가?
	if (item->IsExchanging())
	{
		SPDLOG_DEBUG("EXCHANGE under exchanging");
		return false;
	}

	if (!m_pGrid->IsEmpty(display_pos, 1, item->GetSize()))
	{
		SPDLOG_DEBUG("EXCHANGE not empty item_pos {} {} {}", display_pos, 1, item->GetSize());
		return false;
	}

	Accept(false);
	GetCompany()->Accept(false);

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (m_apItems[i])
			continue;

		m_apItems[i]		= item;
		m_aItemPos[i]		= item_pos;
		m_abItemDisplayPos[i]	= display_pos;
		m_pGrid->Put(display_pos, 1, item->GetSize());

		item->SetExchanging(true);

		exchange_packet(m_pOwner, 
				EXCHANGE_SUBHEADER_GC_ITEM_ADD,
				true,
				item->GetVnum(),
				TItemPos(RESERVED_WINDOW, display_pos),
				item->GetCount(),
				item);

		exchange_packet(GetCompany()->GetOwner(),
				EXCHANGE_SUBHEADER_GC_ITEM_ADD, 
				false, 
				item->GetVnum(),
				TItemPos(RESERVED_WINDOW, display_pos),
				item->GetCount(),
				item);

		SPDLOG_DEBUG("EXCHANGE AddItem success {} pos({}, {}) {}", item->GetName(), item_pos.window_type, item_pos.cell, display_pos);

		return true;
	}

	// 추가할 공간이 없음
	return false;
}

bool CExchange::RemoveItem(BYTE pos)
{
	if (pos >= EXCHANGE_ITEM_MAX_NUM)
		return false;

	if (!m_apItems[pos])
		return false;

	TItemPos PosOfInventory = m_aItemPos[pos];
	m_apItems[pos]->SetExchanging(false);

	m_pGrid->Get(m_abItemDisplayPos[pos], 1, m_apItems[pos]->GetSize());

	exchange_packet(GetOwner(),	EXCHANGE_SUBHEADER_GC_ITEM_DEL, true, pos, NPOS, 0);
	exchange_packet(GetCompany()->GetOwner(), EXCHANGE_SUBHEADER_GC_ITEM_DEL, false, pos, PosOfInventory, 0);

	Accept(false);
	GetCompany()->Accept(false);

	m_apItems[pos]	    = NULL;
	m_aItemPos[pos]	    = NPOS;
	m_abItemDisplayPos[pos] = 0;
	return true;
}

bool CExchange::AddGold(int gold)
{
	if (gold <= 0)
		return false;

	if (GetOwner()->GetGold() < gold)
	{
		// 가지고 있는 돈이 부족.
		exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_LESS_GOLD, 0, 0, NPOS, 0);
		return false;
	}

	if ( m_lGold > 0 )
	{
		return false;
	}
	

	Accept(false);
	GetCompany()->Accept(false);

	m_lGold = gold;

	exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_GOLD_ADD, true, m_lGold, NPOS, 0);
	exchange_packet(GetCompany()->GetOwner(), EXCHANGE_SUBHEADER_GC_GOLD_ADD, false, m_lGold, NPOS, 0);
	return true;
}

// 돈이 충분히 있는지, 교환하려는 아이템이 실제로 있는지 확인 한다.
bool CExchange::Check(int * piItemCount)
{
	if (GetOwner()->GetGold() < m_lGold)
		return false;

	int item_count = 0;

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (!m_apItems[i])
			continue;

		if (!m_aItemPos[i].IsValidItemPosition())
			return false;

		if (m_apItems[i] != GetOwner()->GetItem(m_aItemPos[i]))
			return false;

		++item_count;
	}

	*piItemCount = item_count;
	return true;
}

bool CExchange::CheckSpace()
{
	static CGrid s_grid1(5, INVENTORY_MAX_NUM/5 / 2); // inven page 1
	static CGrid s_grid2(5, INVENTORY_MAX_NUM/5 / 2); // inven page 2

	s_grid1.Clear();
	s_grid2.Clear();

	LPCHARACTER	victim = GetCompany()->GetOwner();
	LPITEM item;

	int i;

	for (i = 0; i < INVENTORY_MAX_NUM / 2; ++i)
	{
		if (!(item = victim->GetInventoryItem(i)))
			continue;

		s_grid1.Put(i, 1, item->GetSize());
	}
	for (i = INVENTORY_MAX_NUM / 2; i < INVENTORY_MAX_NUM; ++i)
	{
		if (!(item = victim->GetInventoryItem(i)))
			continue;

		s_grid2.Put(i - INVENTORY_MAX_NUM / 2, 1, item->GetSize());
	}

	// 아... 뭔가 개병신 같지만... 용혼석 인벤을 노멀 인벤 보고 따라 만든 내 잘못이다 ㅠㅠ
	static std::vector <WORD> s_vDSGrid(DRAGON_SOUL_INVENTORY_MAX_NUM);
	
	// 일단 용혼석을 교환하지 않을 가능성이 크므로, 용혼석 인벤 복사는 용혼석이 있을 때 하도록 한다.
	bool bDSInitialized = false;
	
	for (i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (!(item = m_apItems[i]))
			continue;

		if (item->IsDragonSoul())
		{
			if (!victim->DragonSoul_IsQualified())
			{
				return false;
			}

			if (!bDSInitialized)
			{
				bDSInitialized = true;
				victim->CopyDragonSoulItemGrid(s_vDSGrid);
			}

			bool bExistEmptySpace = false;
			WORD wBasePos = DSManager::instance().GetBasePosition(item);
			if (wBasePos >= DRAGON_SOUL_INVENTORY_MAX_NUM)
				return false;
			
			for (int i = 0; i < DRAGON_SOUL_BOX_SIZE; i++)
			{
				WORD wPos = wBasePos + i;
				if (0 == s_vDSGrid[wBasePos])
				{
					bool bEmpty = true;
					for (int j = 1; j < item->GetSize(); j++)
					{
						if (s_vDSGrid[wPos + j * DRAGON_SOUL_BOX_COLUMN_NUM])
						{
							bEmpty = false;
							break;
						}
					}
					if (bEmpty)
					{
						for (int j = 0; j < item->GetSize(); j++)
						{
							s_vDSGrid[wPos + j * DRAGON_SOUL_BOX_COLUMN_NUM] =  wPos + 1;
						}
						bExistEmptySpace = true;
						break;
					}
				}
				if (bExistEmptySpace)
					break;
			}
			if (!bExistEmptySpace)
				return false;
		}
		else
		{
			int iPos = s_grid1.FindBlank(1, item->GetSize());

			if (iPos >= 0)
			{
				s_grid1.Put(iPos, 1, item->GetSize());
			}
			else
			{
				iPos = s_grid2.FindBlank(1, item->GetSize());

				if (iPos >= 0)
				{
					s_grid2.Put(iPos, 1, item->GetSize());
				}
				else
				{
					return false;
				}
			}
		}
	}

	return true;
}

// 교환 끝 (아이템과 돈 등을 실제로 옮긴다)
bool CExchange::Done()
{
	int		empty_pos, i;
	LPITEM	item;

	LPCHARACTER	victim = GetCompany()->GetOwner();

	for (i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (!(item = m_apItems[i]))
			continue;

		if (item->IsDragonSoul())
			empty_pos = victim->GetEmptyDragonSoulInventory(item);
		else
			empty_pos = victim->GetEmptyInventory(item->GetSize());

		if (empty_pos < 0)
		{
			SPDLOG_ERROR("Exchange::Done : Cannot find blank position in inventory {} <-> {} item {}",
					m_pOwner->GetName(), victim->GetName(), item->GetName());
			continue;
		}

		assert(empty_pos >= 0);

		m_pOwner->SyncQuickslot(QUICKSLOT_TYPE_ITEM, item->GetCell(), 255);

		item->RemoveFromCharacter();
		if (item->IsDragonSoul())
			item->AddToCharacter(victim, TItemPos(DRAGON_SOUL_INVENTORY, empty_pos));
		else
			item->AddToCharacter(victim, TItemPos(INVENTORY, empty_pos));
		ITEM_MANAGER::instance().FlushDelayedSave(item);

		item->SetExchanging(false);
		{
			char exchange_buf[51];

			snprintf(exchange_buf, sizeof(exchange_buf), "%s %u %u", item->GetName(), GetOwner()->GetPlayerID(), item->GetCount());
			LogManager::instance().ItemLog(victim, item, "EXCHANGE_TAKE", exchange_buf);

			snprintf(exchange_buf, sizeof(exchange_buf), "%s %u %u", item->GetName(), victim->GetPlayerID(), item->GetCount());
			LogManager::instance().ItemLog(GetOwner(), item, "EXCHANGE_GIVE", exchange_buf);

			if (item->GetVnum() >= 80003 && item->GetVnum() <= 80007)
			{
				LogManager::instance().GoldBarLog(victim->GetPlayerID(), item->GetID(), EXCHANGE_TAKE, "");
				LogManager::instance().GoldBarLog(GetOwner()->GetPlayerID(), item->GetID(), EXCHANGE_GIVE, "");
			}
		}

		m_apItems[i] = NULL;
	}

	if (m_lGold)
	{
		GetOwner()->PointChange(POINT_GOLD, -m_lGold, true);
		victim->PointChange(POINT_GOLD, m_lGold, true);

		if (m_lGold > 1000)
		{
			char exchange_buf[51];
			snprintf(exchange_buf, sizeof(exchange_buf), "%u %s", GetOwner()->GetPlayerID(), GetOwner()->GetName());
			LogManager::instance().CharLog(victim, m_lGold, "EXCHANGE_GOLD_TAKE", exchange_buf);

			snprintf(exchange_buf, sizeof(exchange_buf), "%u %s", victim->GetPlayerID(), victim->GetName());
			LogManager::instance().CharLog(GetOwner(), m_lGold, "EXCHANGE_GOLD_GIVE", exchange_buf);
		}
	}

	m_pGrid->Clear();
	return true;
}

// 교환을 동의
bool CExchange::Accept(bool bAccept)
{
	if (m_bAccept == bAccept)
		return true;

	m_bAccept = bAccept;

	// 둘 다 동의 했으므로 교환 성립
	if (m_bAccept && GetCompany()->m_bAccept)
	{
		int	iItemCount;

		LPCHARACTER victim = GetCompany()->GetOwner();

		//PREVENT_PORTAL_AFTER_EXCHANGE
		GetOwner()->SetExchangeTime();
		victim->SetExchangeTime();		
		//END_PREVENT_PORTAL_AFTER_EXCHANGE

		// exchange_check 에서는 교환할 아이템들이 제자리에 있나 확인하고,
		// 엘크도 충분히 있나 확인한다, 두번째 인자로 교환할 아이템 개수
		// 를 리턴한다.
		if (!Check(&iItemCount))
		{
			GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Not enough Yang or not enough space in the inventory."));
			victim->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The other person does not have enough Yang or does not have any space left in their inventory."));
			goto EXCHANGE_END;
		}

		// 리턴 받은 아이템 개수로 상대방의 소지품에 남은 자리가 있나 확인한다.
		if (!CheckSpace())
		{
			GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The other person has no space left in their inventory."));
			victim->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There isn't enough space in your inventory."));
			goto EXCHANGE_END;
		}

		// 상대방도 마찬가지로..
		if (!GetCompany()->Check(&iItemCount))
		{
			victim->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Not enough Yang or not enough space in the inventory."));
			GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The other person does not have enough Yang or does not have any space left in their inventory."));
			goto EXCHANGE_END;
		}

		if (!GetCompany()->CheckSpace())
		{
			victim->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The other person has no space left in their inventory."));
			GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There isn't enough space in your inventory."));
			goto EXCHANGE_END;
		}

		if (!db_clientdesc->IsPhase(PHASE_DBCLIENT))
		{
			SPDLOG_ERROR("Cannot use exchange feature while DB cache connection is dead.");
			victim->ChatPacket(CHAT_TYPE_INFO, "Unknown error");
			GetOwner()->ChatPacket(CHAT_TYPE_INFO, "Unknown error");
			goto EXCHANGE_END;
		}

		if (Done())
		{
			if (m_lGold) // 돈이 있을 ??만 저장
				GetOwner()->Save();

			if (GetCompany()->Done())
			{
				if (GetCompany()->m_lGold) // 돈이 있을 때만 저장
					victim->Save();

				// INTERNATIONAL_VERSION
				GetOwner()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The trade with %s has been successful."), victim->GetName());
				victim->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The trade with %s has been successful."), GetOwner()->GetName());
				// END_OF_INTERNATIONAL_VERSION
			}
		}

EXCHANGE_END:
		Cancel();
		return false;
	}
	else
	{
		// 아니면 accept에 대한 패킷을 보내자.
		exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_ACCEPT, true, m_bAccept, NPOS, 0);
		exchange_packet(GetCompany()->GetOwner(), EXCHANGE_SUBHEADER_GC_ACCEPT, false, m_bAccept, NPOS, 0);
		return true;
	}
}

// 교환 취소
void CExchange::Cancel()
{
	exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_END, 0, 0, NPOS, 0);
	GetOwner()->SetExchange(NULL);

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (m_apItems[i])
			m_apItems[i]->SetExchanging(false);
	}

	if (GetCompany())
	{
		GetCompany()->SetCompany(NULL);
		GetCompany()->Cancel();
	}

	M2_DELETE(this);
}

