#include "stdafx.h"
#include "PeerBase.h"

#include <event2/buffer.h>

CPeerBase::CPeerBase() : m_bufferevent(nullptr)
{
}

CPeerBase::~CPeerBase()
{
	Destroy();
}

void CPeerBase::Destroy()
{
    if (m_bufferevent) {
        bufferevent_free(m_bufferevent);
        m_bufferevent = nullptr;
    }
}

bool CPeerBase::Accept(bufferevent* bufev, sockaddr* addr)
{
    if (!bufev) {
        SPDLOG_ERROR("Cannot accept empty bufferevent!");
        return false;
    }

    if (m_bufferevent != nullptr) {
        SPDLOG_ERROR("Peer is already initialized");
        return false;
    }

    // Save the bufferevent
    m_bufferevent = bufev;

    // Get the address of the conected peer
    sockaddr_in* peer;
    sockaddr_in6* peer6;

    switch (addr->sa_family) {
        case AF_INET:
            peer = (sockaddr_in*) addr;
            inet_ntop(AF_INET, &(peer->sin_addr), m_host, INET_ADDRSTRLEN);
            break;

        case AF_INET6:
            peer6 = (sockaddr_in6*) addr;
            inet_ntop(AF_INET, &(peer6->sin6_addr), m_host, INET6_ADDRSTRLEN);
            break;

        default:
            break;
    }

    // Trigger the OnAccept event
	OnAccept();

	SPDLOG_DEBUG("ACCEPT FROM {}", m_host);

	return true;
}

void CPeerBase::Close()
{
	OnClose();
}

void CPeerBase::EncodeBYTE(BYTE b)
{
	Encode(&b, sizeof(b));
}

void CPeerBase::EncodeWORD(WORD w)
{
    Encode(&w, sizeof(w));
}

void CPeerBase::EncodeDWORD(DWORD dw)
{
    Encode(&dw, sizeof(dw));
}

void CPeerBase::Encode(const void* data, size_t size)
{
	if (!m_bufferevent)
	{
		SPDLOG_ERROR("Bufferevent not ready!");
		return;
	}

	if (bufferevent_write(m_bufferevent, data, size) != 0) {
        SPDLOG_ERROR("Buffer write error!");
        return;
    }
}

void CPeerBase::RecvEnd(size_t proceed_bytes)
{
    if (!m_bufferevent)
    {
        SPDLOG_ERROR("Bufferevent not ready!");
        return;
    }

    evbuffer *input = bufferevent_get_input(m_bufferevent);
    evbuffer_drain(input, proceed_bytes);
}

size_t CPeerBase::GetRecvLength()
{
    if (!m_bufferevent)
    {
        SPDLOG_ERROR("Bufferevent not ready!");
        return 0;
    }

    evbuffer *input = bufferevent_get_input(m_bufferevent);
	return evbuffer_get_length(input);
}

const void * CPeerBase::GetRecvBuffer(ssize_t ensure_bytes)
{
    if (!m_bufferevent)
    {
        SPDLOG_ERROR("Bufferevent not ready!");
        return nullptr;
    }

    evbuffer *input = bufferevent_get_input(m_bufferevent);
	return evbuffer_pullup(input, ensure_bytes);
}

size_t CPeerBase::GetSendLength()
{
    if (!m_bufferevent)
    {
        SPDLOG_ERROR("Bufferevent not ready!");
        return 0;
    }

    evbuffer *output = bufferevent_get_output(m_bufferevent);
    return evbuffer_get_length(output);
}
