
#include <common/stl.h>

class CMapLocation : public singleton<CMapLocation>
{
	public:
		typedef struct SLocation
		{
			int         addr;
			WORD        port;
		} TLocation;    

		bool    Get(int x, int y, int& lMapIndex, LONG & lAddr, WORD & wPort);
		bool	Get(int iIndex, LONG & lAddr, WORD & wPort);
		void    Insert(int lIndex, const char * c_pszHost, WORD wPort);

	protected:
		std::map<int, TLocation> m_map_address;
};      

