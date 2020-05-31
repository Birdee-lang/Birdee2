#pragma once

#include <stdint.h>

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

struct GenericArray
{
	union {
		struct
		{
			uint32_t sz;
			char cbuf[1];
		}packed;
		struct
		{
			uint32_t sz;
			void* buf[1];
		}unpacked;
	};
};


struct BirdeeString
{
	GenericArray* arr;
	uint32_t sz;
};

struct BirdeeTypeInfo
{
	BirdeeString* name;
	BirdeeTypeInfo* parent;
	GenericArray* impls;
	GenericArray* itables;
	void* vtable[0];
};

struct BirdeeRTTIObject
{
	BirdeeTypeInfo* type;
};