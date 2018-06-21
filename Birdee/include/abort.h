#pragma once
#include <stdlib.h>
inline void BirdeeAssert(bool b, const char* text)
{
	if (!b)
	{
		std::cerr << text << '\n';
		abort();
	}
}