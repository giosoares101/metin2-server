/*
 *    Filename: utils.c
 * Description: 각종 유틸리티
 *
 *      Author: 비엽 aka. Cronan
 */
#define __LIBTHECORE__
#include "stdafx.h"
#include <common/length.h>

static struct timeval null_time = { 0, 0 };

struct timeval * timediff(const struct timeval *a, const struct timeval *b)
{
    static struct timeval rslt;

    if (a->tv_sec < b->tv_sec)
	return &null_time;
    else if (a->tv_sec == b->tv_sec)
    {
	if (a->tv_usec < b->tv_usec)
	    return &null_time;
	else
	{
	    rslt.tv_sec         = 0;
	    rslt.tv_usec        = a->tv_usec - b->tv_usec;
	    return &rslt;
	}
    }
    else
    {                      /* a->tv_sec > b->tv_sec */
	rslt.tv_sec = a->tv_sec - b->tv_sec;

	if (a->tv_usec < b->tv_usec)
	{
	    rslt.tv_usec = a->tv_usec + 1000000 - b->tv_usec;
	    rslt.tv_sec--;
	} else
	    rslt.tv_usec = a->tv_usec - b->tv_usec;

	return &rslt;
    }
}

struct timeval * timeadd(struct timeval *a, struct timeval *b)
{
    static struct timeval rslt;

    rslt.tv_sec         = a->tv_sec + b->tv_sec;
    rslt.tv_usec        = a->tv_usec + b->tv_usec;

    while (rslt.tv_usec >= 1000000)
    {
	rslt.tv_usec -= 1000000;
	rslt.tv_sec++;
    }

    return &rslt;
}


char *time_str(time_t ct)
{
    static char * time_s;

    time_s = asctime(localtime(&ct));

    time_s[strlen(time_s) - 6] = '\0';
    time_s += 4;

    return (time_s);
}

void trim_and_lower(const char * src, char * dest, size_t dest_size)
{
    const char * tmp = src;
    size_t len = 0;

    if (!dest || dest_size == 0)
	return;

    if (!src)
    {
	*dest = '\0';
	return;
    }

    // 앞에 빈칸 건너 뛰기
    while (*tmp)
    {
	if (!isspace(*tmp))
	    break;

	tmp++;
    }

    // \0 확보
    --dest_size;

    while (*tmp && len < dest_size)
    {
	*(dest++) = LOWER(*tmp); // LOWER는 매크로라 ++ 쓰면 안됨
	++tmp;
	++len;
    }

    *dest = '\0';

    if (len > 0)
    {
	// 뒤에 빈칸 지우기
	--dest;

	while (*dest && isspace(*dest) && len--)
	    *(dest--) = '\0';
    }
}

/* "Name : 비엽" 과 같이 "항목 : 값" 으로 이루어진 문자열에서 
   항목을 token 으로, 값을 value 로 복사하여 리턴한다. */
void parse_token(char *src, char *token, char *value)
{
    char *tmp;

    for (tmp = src; *tmp && *tmp != ':'; tmp++)
    {
	if (isspace(*tmp))
	    continue;

	*(token++) = LOWER(*tmp);
    }

    *token = '\0';

    for (tmp += 2; *tmp; tmp++)
    {
	if (*tmp == '\n' || *tmp == '\r')
	    continue;

	*(value++) = *tmp;
    }   

    *value = '\0';
}   


struct tm * tm_calc(const struct tm * curr_tm, int days)
{
    bool                yoon = false;
    static struct tm    new_tm;
    int                 monthdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (!curr_tm)
    {       
	time_t time_s = time(0);
	new_tm = *localtime(&time_s);
    }
    else    
	memcpy(&new_tm, curr_tm, sizeof(struct tm));

    if (new_tm.tm_mon == 1)
    {
	if (!((new_tm.tm_year + 1900) % 4))
	{
	    if (!((new_tm.tm_year + 1900) % 100))
	    {
		if (!((new_tm.tm_year + 1900) % 400))
		    yoon = true;
	    }   
	    else
		yoon = true;
	}

	if (yoon)
	    new_tm.tm_mday += 1;
    }

    if (yoon)
	monthdays[1] = 29;
    else
	monthdays[1] = 28;

    new_tm.tm_mday += days;

    if (new_tm.tm_mday <= 0)
    {
	new_tm.tm_mon--;

	if (new_tm.tm_mon < 0)
	{
	    new_tm.tm_year--;
	    new_tm.tm_mon = 11;
	}

	new_tm.tm_mday = monthdays[new_tm.tm_mon] + new_tm.tm_mday;
    }
    else if (new_tm.tm_mday > monthdays[new_tm.tm_mon])
    {
	new_tm.tm_mon++;

	if (new_tm.tm_mon > 11)
	{
	    new_tm.tm_year++;
	    new_tm.tm_mon = 0;
	}

	new_tm.tm_mday = monthdays[new_tm.tm_mon] - new_tm.tm_mday;
    }

    return (&new_tm);
}

#ifndef __WIN32__
void thecore_sleep(struct timeval* timeout)
{
    if (select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, timeout) < 0)
    {
        if (errno != EINTR)
        {
            SPDLOG_ERROR("select sleep {}", strerror(errno));
            return;
        }
    }
}

void thecore_msleep(DWORD dwMillisecond)
{
    static struct timeval tv_sleep;
    tv_sleep.tv_sec = dwMillisecond / 1000;
    tv_sleep.tv_usec = dwMillisecond * 1000;
    thecore_sleep(&tv_sleep);
}

void core_dump_unix(const char *who, WORD line)
{
    SPDLOG_CRITICAL("*** Dumping Core {}:{} ***", who, line);

    fflush(stdout);
    fflush(stderr); 

    if (fork() == 0)
        abort();
}

/*
uint64_t rdtsc()
{
	uint64_t x;
	__asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
	return x;
}
*/

#else

void thecore_sleep(struct timeval* timeout)
{
    Sleep(timeout->tv_sec * 1000 + timeout->tv_usec / 1000);
}

void thecore_msleep(DWORD dwMillisecond)
{
    Sleep(dwMillisecond);
}

void gettimeofday(struct timeval* t, struct timezone* dummy)
{
    DWORD millisec = GetTickCount();

    t->tv_sec = (millisec / 1000);
    t->tv_usec = (millisec % 1000) * 1000;
}

void core_dump_unix(const char *who, WORD line)
{   
}

#endif

float get_float_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    tv.tv_sec -= 1057699978;
    return ((float) tv.tv_sec + ((float) tv.tv_usec / 1000000.0f));
}

static DWORD get_boot_sec()
{
	static struct timeval tv_boot = {0L, 0L};

	if (tv_boot.tv_sec == 0)
		gettimeofday(&tv_boot, NULL);

	return tv_boot.tv_sec;
}

DWORD get_dword_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    //tv.tv_sec -= 1057699978;
    tv.tv_sec -= get_boot_sec();
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

std::string GetSocketHost(const sockaddr * address) {
    sockaddr_in* peer;
    sockaddr_in6* peer6;
    char host[MAX_HOST_LENGTH + 1];

    switch (address->sa_family) {
        case AF_INET:
            peer = (sockaddr_in*) address;
            return inet_ntop(AF_INET, &(peer->sin_addr), host, INET_ADDRSTRLEN);

        case AF_INET6:
            peer6 = (sockaddr_in6*) address;
            return inet_ntop(AF_INET, &(peer6->sin6_addr), host, INET6_ADDRSTRLEN);

        default:
            break;
    }

    return "";
}

in_port_t GetSocketPort(const sockaddr * address) {
    sockaddr_in* peer;
    sockaddr_in6* peer6;

    switch (address->sa_family) {
        case AF_INET:
            peer = (sockaddr_in*) address;
            return peer->sin_port;

        case AF_INET6:
            peer6 = (sockaddr_in6*) address;
            return peer6->sin6_port;

        default:
            break;
    }

    return 0;
}