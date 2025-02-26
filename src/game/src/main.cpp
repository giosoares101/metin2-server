#include "stdafx.h"
#include "constants.h"
#include "config.h"
#include "event.h"
#include "packet.h"
#include "desc_manager.h"
#include "item_manager.h"
#include "char.h"
#include "char_manager.h"
#include "mob_manager.h"
#include "motion.h"
#include "sectree_manager.h"
#include "shop_manager.h"
#include "regen.h"
#include "text_file_loader.h"
#include "skill.h"
#include "pvp.h"
#include "party.h"
#include "questmanager.h"
#include "profiler.h"
#include "lzo_manager.h"
#include "messenger_manager.h"
#include "db.h"
#include "log.h"
#include "p2p.h"
#include "guild_manager.h"
#include "dungeon.h"
#include "cmd.h"
#include "refine.h"
#include "banword.h"
#include "priv_manager.h"
#include "war_map.h"
#include "building.h"
#include "login_sim.h"
#include "target.h"
#include "marriage.h"
#include "wedding.h"
#include "fishing.h"
#include "item_addon.h"
#include "TrafficProfiler.h"
#include "locale_service.h"
#include "arena.h"
#include "OXEvent.h"
#include "monarch.h"
#include "polymorph.h"
#include "blend_item.h"
#include "castle.h"
#include "ani.h"
#include "BattleArena.h"
#include "over9refine.h"
#include "horsename_manager.h"
#include "MarkManager.h"
#include "spam.h"
#include "panama.h"
#include "threeway_war.h"
#include "DragonLair.h"
#include "skill_power.h"
#include "SpeedServer.h"
#include "DragonSoul.h"
#include <version.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/dns.h>

#ifdef __AUCTION__
#include "auction_manager.h"
#endif

#ifdef USE_STACKTRACE
#include <execinfo.h>
#endif

//extern const char * _malloc_options;
#if defined(__FreeBSD__) && defined(DEBUG_ALLOC)
extern void (*_malloc_message)(const char* p1, const char* p2, const char* p3, const char* p4);
// FreeBSD _malloc_message replacement
void WriteMallocMessage(const char* p1, const char* p2, const char* p3, const char* p4) {
	FILE* fp = ::fopen(DBGALLOC_LOG_FILENAME, "a");
	if (fp == NULL) {
		return;
	}
	::fprintf(fp, "%s %s %s %s\n", p1, p2, p3, p4);
	::fclose(fp);
}
#endif

// TRAFFIC_PROFILER
static const DWORD	TRAFFIC_PROFILE_FLUSH_CYCLE = 3600;	///< TrafficProfiler 의 Flush cycle. 1시간 간격
// END_OF_TRAFFIC_PROFILER

// 게임과 연결되는 소켓
volatile int	num_events_called = 0;
int             max_bytes_written = 0;
int             current_bytes_written = 0;
int             total_bytes_written = 0;
BYTE		g_bLogLevel = 0;

evconnlistener *	tcp_listener = nullptr;
evconnlistener *	p2p_listener = nullptr;

event_base *	    ev_base = nullptr;
evdns_base *	    dns_base = nullptr;

static void AcceptError(evconnlistener *listener, void *ctx);
static void AcceptTCPConnection(evconnlistener* listener, evutil_socket_t fd, sockaddr* address, int socklen, void* ctx);
static void AcceptP2PConnection(evconnlistener* listener, evutil_socket_t fd, sockaddr* address, int socklen, void* ctx);

int		io_loop(event_base * base);

int		start(int argc, char **argv);
int		idle();
void	destroy();

enum EProfile
{
	PROF_EVENT,
	PROF_CHR_UPDATE,
	PROF_IO,
	PROF_HEARTBEAT,
	PROF_MAX_NUM
};

static DWORD s_dwProfiler[PROF_MAX_NUM];

int g_shutdown_disconnect_pulse;
int g_shutdown_disconnect_force_pulse;
int g_shutdown_core_pulse;
bool g_bShutdown=false;

extern int speed_server;
#ifdef __AUCTION__
extern int auction_server;
#endif
extern void CancelReloadSpamEvent();

void ContinueOnFatalError()
{
#ifdef USE_STACKTRACE
	void* array[200];
	std::size_t size;
	char** symbols;

	size = backtrace(array, 200);
	symbols = backtrace_symbols(array, size);

	std::ostringstream oss;
	oss << std::endl;
	for (std::size_t i = 0; i < size; ++i) {
		oss << "  Stack> " << symbols[i] << std::endl;
	}

	free(symbols);

	SPDLOG_ERROR("FatalError on {}", oss.str().c_str());
#else
	SPDLOG_ERROR("FatalError");
#endif
}

void ShutdownOnFatalError()
{
	if (!g_bShutdown)
	{
		SPDLOG_CRITICAL("ShutdownOnFatalError!!!!!!!!!!");
		{
			char buf[256];

			strlcpy(buf, LC_TEXT("A critical server error has occurred. The server will restart automatically."), sizeof(buf));
			SendNotice(buf);
			strlcpy(buf, LC_TEXT("You will be disconnected automatically in 10 seconds."), sizeof(buf));
			SendNotice(buf);
			strlcpy(buf, LC_TEXT("You can connect again after 5 minutes."), sizeof(buf));
			SendNotice(buf);
		}

		g_bShutdown = true;
		g_bNoMoreClient = true;

		g_shutdown_disconnect_pulse = thecore_pulse() + PASSES_PER_SEC(10);
		g_shutdown_disconnect_force_pulse = thecore_pulse() + PASSES_PER_SEC(20);
		g_shutdown_core_pulse = thecore_pulse() + PASSES_PER_SEC(30);
	}
}

namespace
{
	struct SendDisconnectFunc
	{
		void operator () (LPDESC d)
		{
			if (d->GetCharacter())
			{
				if (d->GetCharacter()->GetGMLevel() == GM_PLAYER)
					d->GetCharacter()->ChatPacket(CHAT_TYPE_COMMAND, "quit Shutdown(SendDisconnectFunc)");
			}
		}
	};

	struct DisconnectFunc
	{
		void operator () (LPDESC d)
		{
			if (d->GetType() == DESC_TYPE_CONNECTOR)
				return;

			if (d->IsPhase(PHASE_P2P))
				return;

			d->SetPhase(PHASE_CLOSE);
		}
	};
}

extern std::map<DWORD, CLoginSim *> g_sim; // first: AID
extern std::map<DWORD, CLoginSim *> g_simByPID;
extern std::vector<TPlayerTable> g_vec_save;
unsigned int save_idx = 0;

void heartbeat(LPHEART ht, int pulse) 
{
	DWORD t;

	t = get_dword_time();
	num_events_called += event_process(pulse);
	s_dwProfiler[PROF_EVENT] += (get_dword_time() - t);

	t = get_dword_time();

	// 1초마다
	if (!(pulse % ht->passes_per_sec))
	{
		if (!g_bAuthServer)
		{
			TPlayerCountPacket pack;
			pack.dwCount = DESC_MANAGER::instance().GetLocalUserCount();
			db_clientdesc->DBPacket(HEADER_GD_PLAYER_COUNT, 0, &pack, sizeof(TPlayerCountPacket));
		}
		else
		{
			DESC_MANAGER::instance().ProcessExpiredLoginKey();
		}

		{
			int count = 0;
			itertype(g_sim) it = g_sim.begin();

			while (it != g_sim.end())
			{
				if (!it->second->IsCheck())
				{
					it->second->SendLogin();

					if (++count > 50)
					{
						SPDLOG_DEBUG("FLUSH_SENT");
						break;
					}
				}

				it++;
			}

			if (save_idx < g_vec_save.size())
			{
				count = std::min<int>(100, g_vec_save.size() - save_idx);

				for (int i = 0; i < count; ++i, ++save_idx)
					db_clientdesc->DBPacket(HEADER_GD_PLAYER_SAVE, 0, &g_vec_save[save_idx], sizeof(TPlayerTable));

                SPDLOG_DEBUG("SAVE_FLUSH {}", count);
			}
		}
	}

	//
	// 25 PPS(Pulse per second) 라고 가정할 때
	//

	// 약 1.16초마다
	if (!(pulse % (passes_per_sec + 4)))
		CHARACTER_MANAGER::instance().ProcessDelayedSave();

	// 약 5.08초마다
	if (!(pulse % (passes_per_sec * 5 + 2)))
	{
		ITEM_MANAGER::instance().Update();
		DESC_MANAGER::instance().UpdateLocalUserCount();
	}

	s_dwProfiler[PROF_HEARTBEAT] += (get_dword_time() - t);

	DBManager::instance().Process();
	AccountDB::instance().Process();
	CPVPManager::instance().Process();

	if (g_bShutdown)
	{
		if (thecore_pulse() > g_shutdown_disconnect_pulse)
		{
			const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
			std::for_each(c_set_desc.begin(), c_set_desc.end(), ::SendDisconnectFunc());
			g_shutdown_disconnect_pulse = INT_MAX;
		}
		else if (thecore_pulse() > g_shutdown_disconnect_force_pulse)
		{
			const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
			std::for_each(c_set_desc.begin(), c_set_desc.end(), ::DisconnectFunc());
		}
		else if (thecore_pulse() > g_shutdown_disconnect_force_pulse + PASSES_PER_SEC(5))
		{
			thecore_shutdown();
		}
	}
}

static void CleanUpForEarlyExit() {
	CancelReloadSpamEvent();
}

int main(int argc, char **argv)
{
#ifdef DEBUG_ALLOC
	DebugAllocator::StaticSetUp();
#endif

	ilInit(); // DevIL Initialize

	WriteVersion();
    log_init();
	
	SECTREE_MANAGER	sectree_manager;
	CHARACTER_MANAGER	char_manager;
	ITEM_MANAGER	item_manager;
	CShopManager	shop_manager;
	CMobManager		mob_manager;
	CMotionManager	motion_manager;
	CPartyManager	party_manager;
	CSkillManager	skill_manager;
	CPVPManager		pvp_manager;
	LZOManager		lzo_manager;
	DBManager		db_manager;
	AccountDB 		account_db;

	LogManager		log_manager;
	MessengerManager	messenger_manager;
	P2P_MANAGER		p2p_manager;
	CGuildManager	guild_manager;
	CGuildMarkManager mark_manager;
	CDungeonManager	dungeon_manager;
	CRefineManager	refine_manager;
	CBanwordManager	banword_manager;
	CPrivManager	priv_manager;
	CWarMapManager	war_map_manager;
	building::CManager	building_manager;
	CTargetManager	target_manager;
	marriage::CManager	marriage_manager;
	marriage::WeddingManager wedding_manager;
	CItemAddonManager	item_addon_manager;
	CArenaManager arena_manager;
	COXEventManager OXEvent_manager;
	CMonarch		Monarch;
	CHorseNameManager horsename_manager;

	DESC_MANAGER	desc_manager;

	TrafficProfiler	trafficProfiler;
	CTableBySkill SkillPowerByLevel;
	CPolymorphUtils polymorph_utils;
	CProfiler		profiler;
	CBattleArena	ba;
	COver9RefineManager	o9r;
	SpamManager		spam_mgr;
	CThreeWayWar	threeway_war;
	CDragonLairManager	dl_manager;

	CSpeedServerManager SSManager;
	DSManager dsManager;

#ifdef __AUCTION__
	AuctionManager auctionManager;
#endif

	if (!start(argc, argv)) {
		CleanUpForEarlyExit();
		return 0;
	}

	quest::CQuestManager quest_manager;

	if (!quest_manager.Initialize()) {
		CleanUpForEarlyExit();
		return 0;
	}

	MessengerManager::instance().Initialize();
	CGuildManager::instance().Initialize();
	fishing::Initialize();
	OXEvent_manager.Initialize();
	if (speed_server)
		CSpeedServerManager::instance().Initialize();

	Cube_init();
	Blend_Item_init();
	ani_init();
	PanamaLoad();

	if ( g_bTrafficProfileOn )
		TrafficProfiler::instance().Initialize( TRAFFIC_PROFILE_FLUSH_CYCLE, "ProfileLog" );

	// Client PackageCrypt

	//TODO : make it config
	const std::string strPackageCryptInfoDir = "package/";
	if( !desc_manager.LoadClientPackageCryptInfo( strPackageCryptInfoDir.c_str() ) )
	{
		SPDLOG_WARN("Failed to Load ClientPackageCryptInfo Files ({})", strPackageCryptInfoDir);
	}

	while (idle());

	SPDLOG_INFO("<shutdown> Starting...");
	g_bShutdown = true;
	g_bNoMoreClient = true;

	if (g_bAuthServer)
	{
		int iLimit = DBManager::instance().CountQuery() / 50;
		int i = 0;

		do
		{
			DWORD dwCount = DBManager::instance().CountQuery();
			SPDLOG_DEBUG("Queries {}", dwCount);

			if (dwCount == 0)
				break;

			usleep(500000);

			if (++i >= iLimit)
				if (dwCount == DBManager::instance().CountQuery())
					break;
		} while (1);
	}

	SPDLOG_INFO("<shutdown> Destroying CArenaManager...");
	arena_manager.Destroy();
	SPDLOG_INFO("<shutdown> Destroying COXEventManager...");
	OXEvent_manager.Destroy();

	SPDLOG_INFO("<shutdown> Disabling signal timer...");
	signal_timer_disable();

	SPDLOG_INFO("<shutdown> Shutting down CHARACTER_MANAGER...");
	char_manager.GracefulShutdown();
	SPDLOG_INFO("<shutdown> Shutting down ITEM_MANAGER...");
	item_manager.GracefulShutdown();

	SPDLOG_INFO("<shutdown> Flushing db_clientdesc...");
	db_clientdesc->FlushOutput();
	SPDLOG_INFO("<shutdown> Flushing p2p_manager...");
	p2p_manager.FlushOutput();

	SPDLOG_INFO("<shutdown> Destroying CShopManager...");
	shop_manager.Destroy();
	SPDLOG_INFO("<shutdown> Destroying CHARACTER_MANAGER...");
	char_manager.Destroy();
	SPDLOG_INFO("<shutdown> Destroying ITEM_MANAGER...");
	item_manager.Destroy();
	SPDLOG_INFO("<shutdown> Destroying DESC_MANAGER...");
	desc_manager.Destroy();
	SPDLOG_INFO("<shutdown> Destroying quest::CQuestManager...");
	quest_manager.Destroy();
	SPDLOG_INFO("<shutdown> Destroying building::CManager...");
	building_manager.Destroy();

	SPDLOG_INFO("<shutdown> Flushing TrafficProfiler...");
	trafficProfiler.Flush();

	destroy();
    log_destroy();

#ifdef DEBUG_ALLOC
	DebugAllocator::StaticTearDown();
#endif

	return 1;
}

void usage()
{
	printf("Option list\n"
			"-p <port>    : bind port number (port must be over 1024)\n"
			"-l <level>   : sets log level\n"
			"-r           : do not load regen tables\n"
			"-t           : traffic proflie on\n");
}

int start(int argc, char **argv)
{
	std::string st_localeServiceName;

	//_malloc_options = "A";
#if defined(__FreeBSD__) && defined(DEBUG_ALLOC)
	_malloc_message = WriteMallocMessage;
#endif

    int ch;
	while ((ch = getopt(argc, argv, "n:p:erl:tI:")) != -1)
	{
		char* ep = NULL;

		switch (ch)
		{
			case 'I': // IP
				g_szPublicIP = std::string(optarg);

				printf("IP %s\n", g_szPublicIP.c_str());

				break;

			case 'p': // port
				mother_port = strtol(optarg, &ep, 10);

				if (mother_port <= 1024)
				{
					usage();
					return 0;
				}

				printf("port %hu\n", mother_port);

				break;

			case 'l':
				{
					int l = strtol(optarg, &ep, 10);

					log_set_level(l);
                }
				break;

				// LOCALE_SERVICE
			case 'n':
                st_localeServiceName = optarg;
				break;
				// END_OF_LOCALE_SERVICE

			case 'r':
				g_bNoRegen = true;
				break;

				// TRAFFIC_PROFILER
			case 't':
				g_bTrafficProfileOn = true;
				break;
				// END_OF_TRAFFIC_PROFILER

            case '?':
                if (strchr("Ipln", optopt))
                    SPDLOG_ERROR("Option -{} requires an argument.", optopt);
                else if (isprint (optopt))
                    SPDLOG_ERROR("Unknown option `-{}'.", optopt);
                else
                    SPDLOG_ERROR("Unknown option character `\\x{}'.", optopt);
            default:
                usage();
                return 1;
                break;
		}
	}

	// LOCALE_SERVICE
	config_init(st_localeServiceName);
	// END_OF_LOCALE_SERVICE

	bool is_thecore_initialized = thecore_init(25, heartbeat);

	if (!is_thecore_initialized)
	{
        SPDLOG_CRITICAL("Could not initialize thecore, check owner of pid, syslog");
		exit(EXIT_FAILURE);
	}

	if (false == CThreeWayWar::instance().LoadSetting("forkedmapindex.txt"))
	{
		if (false == g_bAuthServer)
		{
            SPDLOG_CRITICAL("Could not Load ThreeWayWar Setting file");
			exit(EXIT_FAILURE);
		}
	}

	signal_timer_disable();

    // Initialize the network stack

    // Check if the public and internal IP addresses were configured
    if (g_szInternalIP.empty()) {
        SPDLOG_CRITICAL("Internal IP address could not be automatically detected. Manually set the IP and try again.");
        exit(EXIT_FAILURE);
    }
    if (g_szPublicIP.empty()) {
        SPDLOG_CRITICAL("Public IP address could not be automatically detected. Manually set the IP and try again.");
        exit(EXIT_FAILURE);
    }

    // Create a new libevent base and listen for new connections
    ev_base = event_base_new();
    if (!ev_base) {
        SPDLOG_CRITICAL("Libevent base initialization FAILED!");
        exit(EXIT_FAILURE);
    }

    dns_base = evdns_base_new(ev_base, 1);
    if (!dns_base) {
        SPDLOG_CRITICAL("Libevent DNS base initialization FAILED!");
        exit(EXIT_FAILURE);
    }

    sockaddr_in sin = {};

    // Main TCP listener
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(g_szPublicBindIP.c_str());
    sin.sin_port = htons(mother_port);

    tcp_listener = evconnlistener_new_bind(
        ev_base,
        AcceptTCPConnection, nullptr,
        LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
        (const sockaddr*)&sin, sizeof(sin)
    );
    if (!tcp_listener) {
        SPDLOG_CRITICAL("TCP listener initialization FAILED!");
        exit(EXIT_FAILURE);
    }
    SPDLOG_INFO("TCP listening on {}:{}", g_szPublicBindIP, mother_port);
    evconnlistener_set_error_cb(tcp_listener, AcceptError);

    // Game P2P listener
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(g_szInternalBindIP.c_str());
    sin.sin_port = htons(p2p_port);

    p2p_listener = evconnlistener_new_bind(
        ev_base,
        AcceptP2PConnection, nullptr,
        LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
        (const sockaddr*)&sin, sizeof(sin)
    );
    if (!p2p_listener) {
        SPDLOG_CRITICAL("P2P listener initialization FAILED!");
        exit(EXIT_FAILURE);
    }
    SPDLOG_INFO("P2P listening on {}:{}", g_szInternalBindIP, p2p_port);
    evconnlistener_set_error_cb(p2p_listener, AcceptError);

    // Create client connections
	db_clientdesc = DESC_MANAGER::instance().CreateConnectionDesc(ev_base, dns_base, db_addr, db_port, PHASE_DBCLIENT, true);
	if (!g_bAuthServer) {
		db_clientdesc->UpdateChannelStatus(0, true);
	}

	if (g_bAuthServer)
	{
		if (g_stAuthMasterIP.length() != 0)
		{
            SPDLOG_INFO("SlaveAuth");
			g_pkAuthMasterDesc = DESC_MANAGER::instance().CreateConnectionDesc(ev_base, dns_base, g_stAuthMasterIP.c_str(), g_wAuthMasterPort, PHASE_P2P, true);
			P2P_MANAGER::instance().RegisterConnector(g_pkAuthMasterDesc);
			g_pkAuthMasterDesc->SetP2P(g_wAuthMasterPort, g_bChannel);

		}
		else
		{
            SPDLOG_INFO("MasterAuth");
		}
	}	
	/* game server to spam server */
	else
	{
		extern unsigned int g_uiSpamBlockDuration;
		extern unsigned int g_uiSpamBlockScore;
		extern unsigned int g_uiSpamReloadCycle;

        SPDLOG_INFO("SPAM_CONFIG: duration {} score {} reload cycle {}",
				g_uiSpamBlockDuration, g_uiSpamBlockScore, g_uiSpamReloadCycle);

		extern void LoadSpamDB();
		LoadSpamDB();
	}

	signal_timer_enable(30);
	return 1;
}

void destroy()
{
    SPDLOG_INFO("<shutdown> Canceling ReloadSpamEvent...");
	CancelReloadSpamEvent();

    SPDLOG_INFO("<shutdown> regen_free()...");
	regen_free();

    SPDLOG_INFO("<shutdown> Closing network stack...");
    if (tcp_listener) {
        evconnlistener_free(tcp_listener);
        tcp_listener = nullptr;
    }

    if (p2p_listener) {
        evconnlistener_free(p2p_listener);
		p2p_listener = nullptr;
    }

    if (dns_base) {
        evdns_base_free(dns_base, 0);
		dns_base = nullptr;
    }

    if (ev_base) {
        event_base_free(ev_base);
        ev_base = nullptr;
    }

    SPDLOG_INFO("<shutdown> event_destroy()...");
	event_destroy();

    SPDLOG_INFO("<shutdown> CTextFileLoader::DestroySystem()...");
	CTextFileLoader::DestroySystem();

    SPDLOG_INFO("<shutdown> thecore_destroy()...");
	thecore_destroy();
}

int idle()
{
	static struct timeval	pta = { 0, 0 };
	static int			process_time_count = 0;
	struct timeval		now;

	if (pta.tv_sec == 0)
		gettimeofday(&pta, (struct timezone *) 0);

	int passed_pulses;

	if (!(passed_pulses = thecore_idle()))
		return 0;

	assert(passed_pulses > 0);

	DWORD t;

	while (passed_pulses--) {
		heartbeat(thecore_heart, ++thecore_heart->pulse);

		// To reduce the possibility of abort() in checkpointing
		thecore_tick();
	}

	t = get_dword_time();
	CHARACTER_MANAGER::instance().Update(thecore_heart->pulse);
	db_clientdesc->Update(t);
	s_dwProfiler[PROF_CHR_UPDATE] += (get_dword_time() - t);

	t = get_dword_time();
	if (!io_loop(ev_base)) return 0;
	s_dwProfiler[PROF_IO] += (get_dword_time() - t);

	gettimeofday(&now, (struct timezone *) 0);
	++process_time_count;

	if (now.tv_sec - pta.tv_sec > 0)
	{
		SPDLOG_TRACE("[{:3}] event {:5}/{:<5} idle {:<4} event {:<4} heartbeat {:<4} I/O {:<4} chrUpate {:<4} | WRITE: {:<7} | PULSE: {}",
				process_time_count,
				num_events_called,
				event_count(),
				thecore_profiler[PF_IDLE],
				s_dwProfiler[PROF_EVENT],
				s_dwProfiler[PROF_HEARTBEAT],
				s_dwProfiler[PROF_IO],
				s_dwProfiler[PROF_CHR_UPDATE],
				current_bytes_written,
				thecore_pulse());

		num_events_called = 0;
		current_bytes_written = 0;

		process_time_count = 0; 
		gettimeofday(&pta, (struct timezone *) 0);

		memset(&thecore_profiler[0], 0, sizeof(thecore_profiler));
		memset(&s_dwProfiler[0], 0, sizeof(s_dwProfiler));
	}

#ifdef __WIN32__
	if (_kbhit()) {
		int c = _getch();
		switch (c) {
			case 0x1b: // Esc
				return 0; // shutdown
				break;
			default:
				break;
		}
	}
#endif

	return 1;
}

static void AcceptError(evconnlistener *listener, void *ctx) {
    struct event_base *base = evconnlistener_get_base(listener);
    int err = EVUTIL_SOCKET_ERROR();
    SPDLOG_CRITICAL("Got an error {} ({}) on the listener. Shutting down.", err, evutil_socket_error_to_string(err));

    event_base_loopexit(base, nullptr);
    ShutdownOnFatalError();
}

static void AcceptTCPConnection(evconnlistener* listener, evutil_socket_t fd, sockaddr* address, int socklen, void* ctx)
{
    // Initialize the peer
    DESC_MANAGER::instance().AcceptDesc(listener, fd, address);
}

static void AcceptP2PConnection(evconnlistener* listener, evutil_socket_t fd, sockaddr* address, int socklen, void* ctx)
{
    // Initialize the peer
    DESC_MANAGER::instance().AcceptP2PDesc(listener, fd, address);
}

int io_loop(event_base * base)
{
	LPDESC	d;
	int		num_events, event_idx;

	DESC_MANAGER::instance().DestroyClosed(); // PHASE_CLOSE인 접속들을 끊어준다.
	DESC_MANAGER::instance().TryConnect();

    // Process network events
    event_base_loop(base, EVLOOP_NONBLOCK);

	return 1;
}

