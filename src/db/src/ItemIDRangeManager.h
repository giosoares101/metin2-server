// vim:ts=4 sw=4
#ifndef __INC_METIN_II_ITEM_ID_RANGE_MANAGER_H__
#define __INC_METIN_II_ITEM_ID_RANGE_MANAGER_H__

namespace {
    static const uint32_t cs_dwMaxItemID = 4290000000UL;
    static const uint32_t cs_dwMinimumRange = 10000000UL;
    static const uint32_t cs_dwMinimumRemainCount = 10000UL;
}

class CItemIDRangeManager : public singleton<CItemIDRangeManager>
{
	private:
		std::list<TItemIDRangeTable> m_listData;

	public:
		CItemIDRangeManager();

		void Build();
		bool BuildRange(DWORD dwMin, DWORD dwMax, TItemIDRangeTable& range);
		void UpdateRange(DWORD dwMin, DWORD dwMax);

		TItemIDRangeTable GetRange();
};

#endif
