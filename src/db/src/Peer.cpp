#include "stdafx.h"
#include "Peer.h"
#include "ItemIDRangeManager.h"

CPeer::CPeer()
{
	m_state = 0;
	m_bChannel = 0;
	m_dwHandle = 0;
	m_dwUserCount = 0;
	m_wListenPort = 0;
	m_wP2PPort = 0;

	memset(m_alMaps, 0, sizeof(m_alMaps));

	m_itemRange.dwMin = m_itemRange.dwMax = m_itemRange.dwUsableItemIDMin = 0;
	m_itemSpareRange.dwMin = m_itemSpareRange.dwMax = m_itemSpareRange.dwUsableItemIDMin = 0;
}

CPeer::~CPeer()
{
	Close();
}

void CPeer::OnAccept()
{
	m_state = STATE_PLAYING;

	static DWORD current_handle = 0;
	m_dwHandle = ++current_handle;

	SPDLOG_DEBUG("Connection accepted. (host: {} handle: {})", m_host, m_dwHandle);
}

void CPeer::OnClose()
{
	m_state = STATE_CLOSE;

	SPDLOG_DEBUG("Connection closed. (host: {})", m_host);
	SPDLOG_DEBUG("ItemIDRange: returned. {} ~ {}", m_itemRange.dwMin, m_itemRange.dwMax);

	CItemIDRangeManager::instance().UpdateRange(m_itemRange.dwMin, m_itemRange.dwMax);

	m_itemRange.dwMin = 0;
	m_itemRange.dwMax = 0;
	m_itemRange.dwUsableItemIDMin = 0;
}

DWORD CPeer::GetHandle()
{
	return m_dwHandle;
}

DWORD CPeer::GetUserCount()
{
	return m_dwUserCount;
}

void CPeer::SetUserCount(DWORD dwCount)
{
	m_dwUserCount = dwCount;
}

bool CPeer::PeekPacket(int & iBytesProceed, BYTE & header, DWORD & dwHandle, DWORD & dwLength, const char ** data)
{
    // Return if not enough data was received to read the header
	if (GetRecvLength() < iBytesProceed + 9)
		return false;

	const char * buf = (const char *) GetRecvBuffer(iBytesProceed + 9);
    if (!buf) {
        SPDLOG_ERROR("PeekPacket: Failed to get network buffer!");
        return false;
    }

	buf += iBytesProceed;

    // Read the header data
	header	= *(buf++);

	dwHandle	= *((DWORD *) buf);
	buf		+= sizeof(DWORD);

	dwLength	= *((DWORD *) buf);
	buf		+= sizeof(DWORD);

    // Ensure that all the data was fully received
	if (iBytesProceed + dwLength + 9 > (DWORD) GetRecvLength())
	{
		SPDLOG_DEBUG("PeekPacket: not enough buffer size: len {}, recv {}",
				9+dwLength, GetRecvLength()-iBytesProceed);
		return false;
	}

    // Ensure that all the required data is available in a contiguous area
    buf = (const char *) GetRecvBuffer(iBytesProceed + dwLength + 9);
    if (!buf) {
        SPDLOG_ERROR("PeekPacket: Failed to get network buffer!");
        return false;
    }

    // Skip the header
    buf += iBytesProceed + 9;

    // Set the data pointer
	*data = buf;
	iBytesProceed += dwLength + 9;
	return true;
}

void CPeer::EncodeHeader(BYTE header, DWORD dwHandle, DWORD dwSize)
{
	HEADER h;

	SPDLOG_TRACE("EncodeHeader {} handle {} size {}", header, dwHandle, dwSize);

	h.bHeader = header;
	h.dwHandle = dwHandle;
	h.dwSize = dwSize;

	Encode(&h, sizeof(HEADER));
}

void CPeer::EncodeReturn(BYTE header, DWORD dwHandle)
{
	EncodeHeader(header, dwHandle, 0);
}

void CPeer::SetP2PPort(WORD wPort)
{
	m_wP2PPort = wPort;
}

void CPeer::SetMaps(LONG * pl)
{
	memcpy(m_alMaps, pl, sizeof(m_alMaps));
}

void CPeer::SendSpareItemIDRange()
{
	if (m_itemSpareRange.dwMin == 0 || m_itemSpareRange.dwMax == 0 || m_itemSpareRange.dwUsableItemIDMin == 0)
	{
		EncodeHeader(HEADER_DG_ACK_SPARE_ITEM_ID_RANGE, 0, sizeof(TItemIDRangeTable));
		Encode(&m_itemSpareRange, sizeof(TItemIDRangeTable));
	}
	else
	{
		SetItemIDRange(m_itemSpareRange);

		if (SetSpareItemIDRange(CItemIDRangeManager::instance().GetRange()) == false)
		{
			SPDLOG_DEBUG("ItemIDRange: spare range set error");
			m_itemSpareRange.dwMin = m_itemSpareRange.dwMax = m_itemSpareRange.dwUsableItemIDMin = 0;
		}

		EncodeHeader(HEADER_DG_ACK_SPARE_ITEM_ID_RANGE, 0, sizeof(TItemIDRangeTable));
		Encode(&m_itemSpareRange, sizeof(TItemIDRangeTable));
	}
}

bool CPeer::SetItemIDRange(TItemIDRangeTable itemRange)
{
	if (itemRange.dwMin == 0 || itemRange.dwMax == 0 || itemRange.dwUsableItemIDMin == 0) return false;

	m_itemRange = itemRange;
	SPDLOG_DEBUG("ItemIDRange: SET {} {} ~ {} start: {}", GetPublicIP(), m_itemRange.dwMin, m_itemRange.dwMax, m_itemRange.dwUsableItemIDMin);

	return true;
}

bool CPeer::SetSpareItemIDRange(TItemIDRangeTable itemRange)
{
	if (itemRange.dwMin == 0 || itemRange.dwMax == 0 || itemRange.dwUsableItemIDMin == 0) return false;

	m_itemSpareRange = itemRange;
	SPDLOG_DEBUG("ItemIDRange: SPARE SET {} {} ~ {} start: {}", GetPublicIP(), m_itemSpareRange.dwMin, m_itemSpareRange.dwMax,
			m_itemSpareRange.dwUsableItemIDMin);

	return true;
}

bool CPeer::CheckItemIDRangeCollision(TItemIDRangeTable itemRange)
{
	if (m_itemRange.dwMin < itemRange.dwMax && m_itemRange.dwMax > itemRange.dwMin)
	{
		SPDLOG_ERROR("ItemIDRange: Collision!! this {} ~ {} check {} ~ {}",
				m_itemRange.dwMin, m_itemRange.dwMax, itemRange.dwMin, itemRange.dwMax);
		return false;
	}

	if (m_itemSpareRange.dwMin < itemRange.dwMax && m_itemSpareRange.dwMax > itemRange.dwMin)
	{
		SPDLOG_ERROR("ItemIDRange: Collision with spare range this {} ~ {} check {} ~ {}",
				m_itemSpareRange.dwMin, m_itemSpareRange.dwMax, itemRange.dwMin, itemRange.dwMax);
		return false;
	}
	
	return true;
}


