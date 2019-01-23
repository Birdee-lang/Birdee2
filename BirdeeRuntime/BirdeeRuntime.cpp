#include <memory.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <gc.h>
#include "BirdeeRuntime.h"

extern "C" void* BirdeeMallocObj(uint32_t sz, GC_finalization_proc proc)
{
	void* ret= GC_malloc(sz);
	if(proc)
		GC_register_finalizer_no_order(ret, proc, nullptr, nullptr, nullptr);
	return ret;
}



static void* BirdeeAllocArr(va_list args, uint32_t sz,uint32_t cur, uint32_t param_sz)
{
	uint32_t len = va_arg(args, uint32_t);
	if (cur == param_sz)
	{
		void* alloc = GC_malloc(sz*len+8);
		GenericArray* arr = (GenericArray*)alloc;
		arr->packed.sz = len;
		return alloc;
	}

	GenericArray* alloc = (GenericArray*)GC_malloc(sizeof(void*)*len+8); //fix-me : +8 ??
	alloc->unpacked.sz = len;
	for (uint32_t i = 0; i < len; i++)
	{
		alloc->unpacked.buf[i] = BirdeeAllocArr(args, sz, cur + 1, param_sz);
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

extern "C" void prints(char* i)
{
	fputs(i, stdout);
}


extern "C" BirdeeString* BirdeeP2S(void* i)
{
	BirdeeString* ret = (BirdeeString*)BirdeeMallocObj(sizeof(BirdeeString),nullptr);
	int sz = snprintf(nullptr, 0, "%p", i) + 1;
	ret->arr = (GenericArray*)BirdeeMallocArr(1, 1, sz);
	ret->sz = sz - 1;
	int retp = snprintf(ret->arr->packed.cbuf, sz, "%p", i);
	assert(retp > 0 && retp < sz);
	return ret;
}

extern "C" BirdeeString* BirdeeI2S(int i)
{
	BirdeeString* ret=(BirdeeString*)BirdeeMallocObj(sizeof(BirdeeString), nullptr);
	int sz = snprintf(nullptr, 0, "%i", i) + 1;
	ret->arr = (GenericArray*)BirdeeMallocArr(1, 1, sz);
	ret->sz = sz - 1;
	int retp = snprintf(ret->arr->packed.cbuf, sz, "%i", i);
	assert(retp > 0 && retp < sz);
	return ret;
}
