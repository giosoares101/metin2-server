/*
 *    Filename: event.h
 * Description: 이벤트 관련 (timed event)
 *
 *      Author: 김한주 (aka. 비엽, Cronan), 송영진 (aka. myevan, 빗자루)
 */
#pragma once

#include <memory>

#ifdef M2_USE_POOL
#include "pool.h"
#endif

/**
 * Base class for all event info data
 */
struct event_info_data 
{
	event_info_data() {}
	virtual ~event_info_data() {}

#ifdef M2_USE_POOL
	static void* operator new(size_t size) {
		return pool_.Acquire(size);
	}
	static void operator delete(void* p, size_t size) {
		pool_.Release(p, size);
	}
private:
	static MemoryPool pool_;
#endif
};
	
typedef struct event EVENT;
typedef std::shared_ptr<EVENT> LPEVENT;
typedef int (*TEVENTFUNC) (LPEVENT event, int processing_time);

#define EVENTFUNC(name)	int (name) (LPEVENT event, int processing_time)
#define EVENTINFO(name) struct name : public event_info_data

struct TQueueElement;

struct event
{
	event() : func(NULL), info(NULL), q_el(NULL), ref_count(0) {}
	~event() {
		if (info != NULL) {
#ifdef M2_USE_POOL
			delete info;
#else
			M2_DELETE(info);
#endif
		}
	}
	TEVENTFUNC			func;
	event_info_data* 	info;
	TQueueElement *		q_el;
	char				is_force_to_end;
	char				is_processing;

	size_t ref_count;
};

template<class T> // T should be a subclass of event_info_data
T* AllocEventInfo() {
#ifdef M2_USE_POOL
	return new T;
#else
	return M2_NEW T;
#endif
}

extern void		event_destroy();
extern int		event_process(int pulse);
extern int		event_count();

#define event_create(func, info, when) event_create_ex(func, info, when)
extern LPEVENT	event_create_ex(TEVENTFUNC func, event_info_data* info, int when);
extern void		event_cancel(LPEVENT * event);			// 이벤트 취소
extern int		event_processing_time(LPEVENT event);	// 수행 시간 리턴
extern int		event_time(LPEVENT event);			// 남은 시간 리턴
extern void		event_reset_time(LPEVENT event, int when);	// 실행 시간 재 설정
extern void		event_set_verbose(int level);

extern event_info_data* FindEventInfo(DWORD dwID);
extern event_info_data*	event_info(LPEVENT event);
