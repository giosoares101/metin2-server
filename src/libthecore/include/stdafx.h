#ifndef __INC_LIBTHECORE_STDAFX_H__
#define __INC_LIBTHECORE_STDAFX_H__

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cerrno>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <cctype>
#include <climits>
#include <string>
#include <dirent.h>

#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

#include <sys/signal.h>
#include <sys/wait.h>

#include <bsd/string.h>

#include <spdlog/spdlog.h>

#include <pthread.h>
#include <semaphore.h>

#include "typedef.h"
#include "heart.h"
#include "buffer.h"
#include "signals.h"
#include "log.h"
#include "main.h"
#include "utils.h"

#endif // __INC_LIBTHECORE_STDAFX_H__
