#ifndef __INC_METIN_II_GAME_ENTITY_H__
#define __INC_METIN_II_GAME_ENTITY_H__

class SECTREE;

class CEntity
{
	public:
		typedef std::unordered_map<LPENTITY, int> ENTITY_MAP;

	public:
		CEntity();
		virtual	~CEntity();

		virtual void	EncodeInsertPacket(LPENTITY entity) = 0;
		virtual	void	EncodeRemovePacket(LPENTITY entity) = 0;

	protected:
		void			Initialize(int type = -1);
		void			Destroy();


	public:
		void			SetType(int type);
		int				GetType() const;
		bool			IsType(int type) const;

		void			ViewCleanup();
		void			ViewInsert(LPENTITY entity, bool recursive = true);
		void			ViewRemove(LPENTITY entity, bool recursive = true);
		void			ViewReencode();	// 주위 Entity에 패킷을 다시 보낸다.

		int				GetViewAge() const	{ return m_iViewAge;	}

		int			GetX() const		{ return m_pos.x; }
		int			GetY() const		{ return m_pos.y; }
		int			GetZ() const		{ return m_pos.z; }
		const PIXEL_POSITION &	GetXYZ() const		{ return m_pos; }

		void			SetXYZ(int x, int y, int z)		{ m_pos.x = x, m_pos.y = y, m_pos.z = z; }
		void			SetXYZ(const PIXEL_POSITION & pos)	{ m_pos = pos; }

		LPSECTREE		GetSectree() const			{ return m_pSectree;	}
		void			SetSectree(LPSECTREE tree)	{ m_pSectree = tree;	}

		void			UpdateSectree();
		void			PacketAround(const void * data, int bytes, LPENTITY except = NULL);
		void			PacketView(const void * data, int bytes, LPENTITY except = NULL);

		void			BindDesc(LPDESC _d)     { m_lpDesc = _d; }
		LPDESC			GetDesc() const			{ return m_lpDesc; }

		void			SetMapIndex(int l)	{ m_lMapIndex = l; }
		int			GetMapIndex() const	{ return m_lMapIndex; }

		void			SetObserverMode(bool bFlag);
		bool			IsObserverMode() const	{ return m_bIsObserver; }

	protected:
		bool			m_bIsObserver;
		bool			m_bObserverModeChange;
		ENTITY_MAP		m_map_view;
		int			m_lMapIndex;

	private:
		LPDESC			m_lpDesc;

		int			m_iType;
		bool			m_bIsDestroyed;

		PIXEL_POSITION		m_pos;

		int			m_iViewAge;

		LPSECTREE		m_pSectree;
};

#endif
