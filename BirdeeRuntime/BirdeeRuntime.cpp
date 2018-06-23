#include <memory.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern "C" void* BirdeeMallocObj(uint32_t sz)
{
	return malloc(sz);
}



struct GenericArray
{
	uint32_t sz;
	union
	{
		char cbuf[0];
		void* buf[10];
	};
};

struct BirdeeString
{
	GenericArray* arr;
	uint32_t sz;
};

static void* BirdeeAllocArr(va_list args, uint32_t sz,uint32_t cur, uint32_t param_sz)
{
	uint32_t len = va_arg(args, uint32_t);
	if (cur == param_sz)
	{
		void* alloc = malloc(sz*len+8);
		GenericArray* arr = (GenericArray*)alloc;
		arr->sz = len;
		return alloc;
	}

	GenericArray* alloc = (GenericArray*)malloc(sizeof(void*)*len+8); //fix-me : +8 ??
	alloc->sz = len;
	for (uint32_t i = 0; i < len; i++)
	{
		alloc->buf[i] = BirdeeAllocArr(args, sz, cur + 1, param_sz);
	}
	return alloc;

}

extern "C" void* BirdeeMallocArr(uint32_t sz,uint32_t param_sz,...)
{
	va_list args;
	va_start(args, param_sz);
	auto ret = BirdeeAllocArr(args, sz,1, param_sz);
	va_end(args);
	return ret;
}




extern "C" BirdeeString* BirdeeP2S(void* i)
{
	BirdeeString* ret = (BirdeeString*)BirdeeMallocObj(sizeof(BirdeeString));
	int sz = snprintf(nullptr, 0, "%p", i) + 1;
	ret->arr = (GenericArray*)BirdeeMallocArr(1, 1, sz);
	ret->sz = sz - 1;
	int retp = snprintf(ret->arr->cbuf, sz, "%p", i);
	assert(retp > 0 && retp < sz);
	return ret;
}

extern "C" BirdeeString* BirdeeI2S(int i)
{
	BirdeeString* ret=(BirdeeString*)BirdeeMallocObj(sizeof(BirdeeString));
	int sz = snprintf(nullptr, 0, "%i", i) + 1;
	ret->arr = (GenericArray*)BirdeeMallocArr(1, 1, sz);
	ret->sz = sz - 1;
	int retp = snprintf(ret->arr->cbuf, sz, "%i", i);
	assert(retp > 0 && retp < sz);
	return ret;
}
