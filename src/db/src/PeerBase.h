#ifndef __INC_PEERBASE_H__
#define __INC_PEERBASE_H__

#include <event2/bufferevent.h>

class CPeerBase {
protected:
	virtual void	OnAccept() = 0;
	virtual void	OnClose() = 0;

public:
	bool		    Accept(bufferevent* bufev, sockaddr* addr);
	void		    Close();

public:
	CPeerBase();
	virtual ~CPeerBase();

	void		    Destroy();

	bufferevent *	GetBufferevent() { return m_bufferevent; }

	void		    EncodeBYTE(BYTE b);
	void		    EncodeWORD(WORD w);
	void		    EncodeDWORD(DWORD dw);
	void		    Encode(const void* data, size_t size);
	void		    RecvEnd(size_t proceed_bytes);
	size_t          GetRecvLength();
	const void *	GetRecvBuffer(ssize_t ensure_bytes);

    size_t          GetSendLength();

	const char *	GetHost() { return m_host; }

protected:
	char		    m_host[IP_ADDRESS_LENGTH + 1];
    bufferevent *   m_bufferevent;
};

#endif
