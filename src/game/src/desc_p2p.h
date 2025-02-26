#include "desc.h"

class DESC_P2P : public DESC
{
	public:
		virtual ~DESC_P2P();

		virtual void	Destroy();
		virtual void	SetPhase(int iPhase);
		bool		Setup(event_base * evbase, evutil_socket_t fd, const sockaddr * c_rSockAddr);
};

