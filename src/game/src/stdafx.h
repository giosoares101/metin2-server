// Basic features
// Enable or disable memory pooling for specific object types
//#define M2_USE_POOL
// Enable or disable heap allocation debugging
//#define DEBUG_ALLOC

#include "debug_allocator.h"

#include <libthecore/include/stdafx.h>

#include <bsd/string.h>

#include <common/singleton.h>
#include <common/utils.h>
#include <common/service.h>

#include <iostream>
#include <algorithm>
#include <cmath>
#include <list>
#include <map>
#include <set>
#include <queue>
#include <string>
#include <vector>
#include <memory>

#include <cfloat>
#include <unordered_map>
#include <unordered_set>

#include "typedef.h"
#include "locale.hpp"
#include "event.h"

#define PASSES_PER_SEC(sec) ((sec) * passes_per_sec)

#define IN
#define OUT
