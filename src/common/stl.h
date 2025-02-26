#pragma once

#include <vector>
#include <string>
#include <map>
#include <list>
#include <functional>
#include <stack>
#include <set>
#ifdef __GNUC__
#include <ext/functional>
#endif

#ifndef itertype
#define itertype(v) typeof((v).begin())
#endif

inline void stl_lowers(std::string& rstRet)
{
	for (size_t i = 0; i < rstRet.length(); ++i)
		rstRet[i] = (char) tolower(rstRet[i]);
}

struct stringhash
{
	size_t operator () (const std::string & str) const
	{
		const auto * s = (const unsigned char*) str.c_str();
		const auto * end = s + str.size();
		size_t h = 0;

		while (s < end)
		{
			h *= 16777619;
			h ^= *(s++);
		}

		return h;
	}
};
