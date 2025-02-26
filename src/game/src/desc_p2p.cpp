#include "stdafx.h"
#include "desc_p2p.h"
#include "protocol.h"
#include "p2p.h"

DESC_P2P::~DESC_P2P()
{
}

void DESC_P2P::Destroy()
{
	if (m_bufevent == nullptr)
		return;

	P2P_MANAGER::instance().UnregisterAcceptor(this);

	SPDLOG_DEBUG("SYSTEM: closing p2p socket.");

	bufferevent_free(m_bufevent);
    m_bufevent = nullptr;

	// Chain up to base class Destroy()
	DESC::Destroy();
}

bool DESC_P2P::Setup(event_base * evbase, evutil_socket_t fd, const sockaddr * c_rSockAddr)
{
    m_bufevent = bufferevent_socket_new(evbase, fd, BEV_OPT_CLOSE_ON_FREE);
    if (m_bufevent == nullptr) {
        SPDLOG_ERROR("DESC::Setup : Could not set up bufferevent!");
        return false;
    }

    // Set the event handlers for this peer
    bufferevent_setcb(m_bufevent, DescReadHandler, DescWriteHandler, DescEventHandler, (LPDESC) this);

    // Enable the events
    bufferevent_enable(m_bufevent, EV_READ|EV_WRITE);

    m_stHost = GetSocketHost(c_rSockAddr);
    m_wPort = GetSocketPort(c_rSockAddr);

	SetPhase(PHASE_P2P);

	SPDLOG_DEBUG("SYSTEM: new p2p connection from [{}]", m_stHost.c_str());

	return true;
}

void DESC_P2P::SetPhase(int iPhase)
{
	static CInputP2P s_inputP2P;

	switch (iPhase)
	{
		case PHASE_P2P:
			SPDLOG_DEBUG("PHASE_P2P");
			m_pInputProcessor = &s_inputP2P;
			break;

		case PHASE_CLOSE:
			m_pInputProcessor = NULL;
			break;

		default:
			SPDLOG_ERROR("DESC_P2P::SetPhase : Unknown phase");
			break;
	}

	m_iPhase = iPhase;
}

