/*
 *    Filename: main.c
 * Description: 라이브러리 초기화/삭제 등
 *
 *      Author: 비엽 aka. Cronan
 */
#define __LIBTHECORE__
#include "stdafx.h"

LPHEART		thecore_heart = NULL;

volatile int	shutdowned = false;
volatile int	tics = 0;
unsigned int	thecore_profiler[NUM_PF];

int thecore_init(int fps, HEARTFUNC heartbeat_func)
{
#if defined(__WIN32__) || defined(__linux__)
    srand(time(0));
#else
    srandom(time(0) + getpid() + getuid());
    srandomdev();
#endif
    signal_setup();

	thecore_heart = heart_new(1000000 / fps, heartbeat_func);
	return true;
}

void thecore_shutdown()
{
    shutdowned = true;
}

int thecore_idle(void)
{
    thecore_tick();

    if (shutdowned)
		return 0;

	int pulses;
	DWORD t = get_dword_time();

	if (!(pulses = heart_idle(thecore_heart)))
	{
		thecore_profiler[PF_IDLE] += (get_dword_time() - t);
		return 0;
	}

    thecore_profiler[PF_IDLE] += (get_dword_time() - t);
    return pulses;
}

void thecore_destroy(void)
{
}

int thecore_pulse(void)
{
	return (thecore_heart->pulse);
}

float thecore_pulse_per_second(void)
{
	return ((float) thecore_heart->passes_per_sec);
}

float thecore_time(void)
{
	return ((float) thecore_heart->pulse / (float) thecore_heart->passes_per_sec);
}

int thecore_is_shutdowned(void)
{
	return shutdowned;
}

void thecore_tick(void)
{
	++tics;
}
