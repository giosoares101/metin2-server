#include "stdafx.h"
#include "utils.h"
#include "char.h"
#include "char_manager.h"
#include "motion.h"
#include "packet.h"
#include "buffer_manager.h"
#include "unique_item.h"
#include "wedding.h"

#define NEED_TARGET	(1 << 0)
#define NEED_PC		(1 << 1)
#define WOMAN_ONLY	(1 << 2)
#define OTHER_SEX_ONLY	(1 << 3)
#define SELF_DISARM	(1 << 4)
#define TARGET_DISARM	(1 << 5)
#define BOTH_DISARM	(SELF_DISARM | TARGET_DISARM)

struct emotion_type_s
{
	const char *	command;
	const char *	command_to_client;
	int	flag;
	float	extra_delay;
} emotion_types[] = {
	{ "\xC5\xB0\xBD\xBA",	"french_kiss",	NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		2.0f },
	{ "\xBB\xC7\xBB\xC7",	"kiss",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		1.5f },
	{ "\xB5\xFB\xB1\xCD",	"slap",		NEED_PC | SELF_DISARM,				1.5f },
	{ "\xB9\xDA\xBC\xF6",	"clap",		0,						1.0f },
	{ "\xBF\xCD",		"cheer1",	0,						1.0f },
	{ "\xB8\xB8\xBC\xBC",	"cheer2",	0,						1.0f },
	
	// DANCE
	{ "\xB4\xED\xBD\xBA\1",	"dance1",	0,						1.0f },
	{ "\xB4\xED\xBD\xBA\2",	"dance2",	0,						1.0f },
	{ "\xB4\xED\xBD\xBA\3",	"dance3",	0,						1.0f },
	{ "\xB4\xED\xBD\xBA\4",	"dance4",	0,						1.0f },
	{ "\xB4\xED\xBD\xBA\5",	"dance5",	0,						1.0f },
	{ "\xB4\xED\xBD\xBA\6",	"dance6",	0,						1.0f },
	// END_OF_DANCE
	{ "\xC3\xE0\xC7\xCF",	"congratulation",	0,				1.0f	},
	{ "\xBF\xEB\xBC\xAD",	"forgive",			0,				1.0f	},
	{ "\xC8\xAD\xB3\xB2",	"angry",			0,				1.0f	},
	{ "\xC0\xAF\xC8\xA4",	"attractive",		0,				1.0f	},
	{ "\xBD\xBD\xC7\xC4",	"sad",				0,				1.0f	},
	{ "\xBA\xEA\xB2\xF4",	"shy",				0,				1.0f	},
	{ "\xC0\xC0\xBF\xF8",	"cheerup",			0,				1.0f	},
	{ "\xC1\xFA\xC5\xF5",	"banter",			0,				1.0f	},
	{ "\xB1\xE2\xBB\xDD",	"joy",				0,				1.0f	},
	{ "\n",	"\n",		0,						0.0f },
	/*
	//{ "\xC5\xB0\xBD\xBA",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		MOTION_ACTION_FRENCH_KISS,	 1.0f },
	{ "\xBB\xC7\xBB\xC7",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		MOTION_ACTION_KISS,		 1.0f },
	{ "\xB2\xB8\xBE\xC8\xB1\xE2",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		MOTION_ACTION_SHORT_HUG,	 1.0f },
	{ "\xC6\xF7\xBF\xCB",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		MOTION_ACTION_LONG_HUG,		 1.0f },
	{ "\xBE\xEE\xB1\xFA\xB5\xBF\xB9\xAB",	NEED_PC | SELF_DISARM,				MOTION_ACTION_PUT_ARMS_SHOULDER, 0.0f },
	{ "\xC6\xC8\xC2\xAF",		NEED_PC	| WOMAN_ONLY | SELF_DISARM,		MOTION_ACTION_FOLD_ARM,		 0.0f },
	{ "\xB5\xFB\xB1\xCD",		NEED_PC | SELF_DISARM,				MOTION_ACTION_SLAP,		 1.5f },

	{ "\xC8\xD6\xC6\xC4\xB6\xF7",		0,						MOTION_ACTION_CHEER_01,		 0.0f },
	{ "\xB8\xB8\xBC\xBC",		0,						MOTION_ACTION_CHEER_02,		 0.0f },
	{ "\xB9\xDA\xBC\xF6",		0,						MOTION_ACTION_CHEER_03,		 0.0f },

	{ "\xC8\xA3\xC8\xA3",		0,						MOTION_ACTION_LAUGH_01,		 0.0f },
	{ "\xC5\xB1\xC5\xB1",		0,						MOTION_ACTION_LAUGH_02,		 0.0f },
	{ "\xBF\xEC\xC7\xCF\xC7\xCF",		0,						MOTION_ACTION_LAUGH_03,		 0.0f },

	{ "\xBE\xFB\xBE\xFB",		0,						MOTION_ACTION_CRY_01,		 0.0f },
	{ "\xC8\xE6\xC8\xE6",		0,						MOTION_ACTION_CRY_02,		 0.0f },

	{ "\xC0\xCE\xBB\xE7",		0,						MOTION_ACTION_GREETING_01,	0.0f },
	{ "\xB9\xD9\xC0\xCC",		0,						MOTION_ACTION_GREETING_02,	0.0f },
	{ "\xC1\xA4\xC1\xDF\xC0\xCE\xBB\xE7",	0,						MOTION_ACTION_GREETING_03,	0.0f },

	{ "\xBA\xF1\xB3\xAD",		0,						MOTION_ACTION_INSULT_01,	0.0f },
	{ "\xB8\xF0\xBF\xE5",		SELF_DISARM,					MOTION_ACTION_INSULT_02,	0.0f },
	{ "\xBF\xEC\xC0\xA1",		0,						MOTION_ACTION_INSULT_03,	0.0f },

	{ "\xB0\xBC\xBF\xEC\xB6\xD7",		0,						MOTION_ACTION_ETC_01,		0.0f },
	{ "\xB2\xF4\xB4\xF6\xB2\xF4\xB4\xF6",	0,						MOTION_ACTION_ETC_02,		0.0f },
	{ "\xB5\xB5\xB8\xAE\xB5\xB5\xB8\xAE",	0,						MOTION_ACTION_ETC_03,		0.0f },
	{ "\xB1\xDC\xC0\xFB\xB1\xDC\xC0\xFB",	0,						MOTION_ACTION_ETC_04,		0.0f },
	{ "\xC6\xA1",		0,						MOTION_ACTION_ETC_05,		0.0f },
	{ "\xBB\xD7",		0,						MOTION_ACTION_ETC_06,		0.0f },
	 */
};


std::set<std::pair<DWORD, DWORD> > s_emotion_set;

ACMD(do_emotion_allow)
{
	if ( ch->GetArena() )
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this in the duel arena."));
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD	val = 0; str_to_number(val, arg1);
	s_emotion_set.insert(std::make_pair(ch->GetVID(), val));
}

bool CHARACTER_CanEmotion(CHARACTER& rch)
{
	// 결혼식 맵에서는 사용할 수 있다.
	if (marriage::WeddingManager::instance().IsWeddingMap(rch.GetMapIndex()))
		return true;

	// 열정의 가면 착용시 사용할 수 있다.
	if (rch.IsEquipUniqueItem(UNIQUE_ITEM_EMOTION_MASK))
		return true;

	if (rch.IsEquipUniqueItem(UNIQUE_ITEM_EMOTION_MASK2))
		return true;

	return false;
}

ACMD(do_emotion)
{
	int i;
	{
		if (ch->IsRiding())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot express emotions whilst riding a horse."));
			return;
		}
	}

	for (i = 0; *emotion_types[i].command != '\n'; ++i)
	{
		if (!strcmp(cmd_info[cmd].command, emotion_types[i].command))
			break;

		if (!strcmp(cmd_info[cmd].command, emotion_types[i].command_to_client))
			break;
	}

	if (*emotion_types[i].command == '\n')
	{
		SPDLOG_ERROR("cannot find emotion");
		return;
	}

	if (!CHARACTER_CanEmotion(*ch))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can do this when you wear an Emotion Mask."));
		return;
	}

	if (IS_SET(emotion_types[i].flag, WOMAN_ONLY) && SEX_MALE==GET_SEX(ch))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Only women can do this."));
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	LPCHARACTER victim = NULL;

	if (*arg1)
		victim = ch->FindCharacterInView(arg1, IS_SET(emotion_types[i].flag, NEED_PC));

	if (IS_SET(emotion_types[i].flag, NEED_TARGET | NEED_PC))
	{
		if (!victim)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This person does not exist."));
			return;
		}
	}

	if (victim)
	{
		if (!victim->IsPC() || victim == ch)
			return;

		if (victim->IsRiding())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use emotions with a player who is riding on a Horse."));
			return;
		}

		int distance = DISTANCE_APPROX(ch->GetX() - victim->GetX(), ch->GetY() - victim->GetY());

		if (distance < 10)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You are too near."));
			return;
		}

		if (distance > 500)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You are too far away."));
			return;
		}

		if (IS_SET(emotion_types[i].flag, OTHER_SEX_ONLY))
		{
			if (GET_SEX(ch)==GET_SEX(victim))
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This action can only be done with another gender."));
				return;
			}
		}

		if (IS_SET(emotion_types[i].flag, NEED_PC))
		{
			if (s_emotion_set.find(std::make_pair(victim->GetVID(), ch->GetVID())) == s_emotion_set.end())
			{
				if (true == marriage::CManager::instance().IsMarried( ch->GetPlayerID() ))
				{
					const marriage::TMarriage* marriageInfo = marriage::CManager::instance().Get( ch->GetPlayerID() );

					const DWORD other = marriageInfo->GetOther( ch->GetPlayerID() );

					if (0 == other || other != victim->GetPlayerID())
					{
						ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You need your fellow player's approval for this."));
						return;
					}
				}
				else
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You need your fellow player's approval for this."));
					return;
				}
			}

			s_emotion_set.insert(std::make_pair(ch->GetVID(), victim->GetVID()));
		}
	}

	char chatbuf[256+1];
	int len = snprintf(chatbuf, sizeof(chatbuf), "%s %u %u", 
			emotion_types[i].command_to_client,
			(DWORD) ch->GetVID(), victim ? (DWORD) victim->GetVID() : 0);

	if (len < 0 || len >= (int) sizeof(chatbuf))
		len = sizeof(chatbuf) - 1;

	++len;  // \0 문자 포함

	TPacketGCChat pack_chat;
	pack_chat.header = HEADER_GC_CHAT;
	pack_chat.size = sizeof(TPacketGCChat) + len;
	pack_chat.type = CHAT_TYPE_COMMAND;
	pack_chat.id = 0;
	TEMP_BUFFER buf;
	buf.write(&pack_chat, sizeof(TPacketGCChat));
	buf.write(chatbuf, len);

	ch->PacketAround(buf.read_peek(), buf.size());

	if (victim)
		SPDLOG_DEBUG("ACTION: {} TO {}", emotion_types[i].command, victim->GetName());
	else
		SPDLOG_DEBUG("ACTION: {}", emotion_types[i].command);
}

