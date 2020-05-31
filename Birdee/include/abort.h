#pragma once
#include "CompileError.h"
inline void BirdeeAssert(bool b, const char* text)
{
	if (!b)
	{
		throw Birdee::CompileError(text);
	}
}