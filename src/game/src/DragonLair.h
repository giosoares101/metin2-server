
#include <unordered_map>

#include <common/stl.h>

class CDragonLair
{
	public:
		CDragonLair (DWORD dwGuildID, int BaseMapID, int PrivateMapID);
		virtual ~CDragonLair ();

		DWORD GetEstimatedTime () const;

		void OnDragonDead (LPCHARACTER pDragon);

	private:
		DWORD StartTime_;
		DWORD GuildID_;
		int BaseMapIndex_;
		int PrivateMapIndex_;
};

class CDragonLairManager : public singleton<CDragonLairManager>
{
	public:
		CDragonLairManager ();
		virtual ~CDragonLairManager ();

		bool Start (int MapIndexFrom, int BaseMapIndex, DWORD GuildID);
		void OnDragonDead (LPCHARACTER pDragon, DWORD KillerGuildID);

		size_t GetLairCount () const { return LairMap_.size(); }

	private:
		std::unordered_map<DWORD, CDragonLair*> LairMap_;
};

