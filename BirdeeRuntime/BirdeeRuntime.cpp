#include <memory.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <gc.h>
#include "BirdeeRuntime.h"

extern "C" void* birdee_0____create__basic__exception__no__call(int ty);
extern "C" void __Birdee_Throw(BirdeeRTTIObject* obj);
#ifndef _WIN32
#include <signal.h>
#include <unistd.h>

static void SigSEGVHandler(int iSignal, siginfo_t * psSiginfo, void * psContext)
{
	sigset_t x;
	sigemptyset(&x);
	sigaddset(&x, SIGSEGV);
	sigprocmask(SIG_UNBLOCK, &x, NULL);
	__Birdee_Throw((BirdeeRTTIObject*)birdee_0____create__basic__exception__no__call(0));
}

static void SigSIGFPEHandler(int iSignal, siginfo_t * psSiginfo, void * psContext)
{
	if (psSiginfo->si_code == FPE_INTDIV || psSiginfo->si_code == FPE_INTDIV)
	{
		sigset_t x;
		sigemptyset(&x);
		sigaddset(&x, SIGFPE);
		sigprocmask(SIG_UNBLOCK, &x, NULL);
		__Birdee_Throw((BirdeeRTTIObject*)birdee_0____create__basic__exception__no__call(1));
	}
	else
	{
		fprintf(stderr, "unhandled SIGFPE\n");
		_exit(3);
	}
}

static void Init()
{
	struct sigaction old_sig;
	struct sigaction new_sig;

	memset(&new_sig, 0, sizeof(new_sig));
	new_sig.sa_sigaction = SigSEGVHandler;
	new_sig.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &new_sig, &old_sig);

	memset(&new_sig, 0, sizeof(new_sig));
	new_sig.sa_sigaction = SigSIGFPEHandler;
	new_sig.sa_flags = SA_SIGINFO;
	sigaction(SIGFPE, &new_sig, &old_sig);
}
#else
#include <eh.h>
#include <Windows.h>
void Init()
{
	SetUnhandledExceptionFilter([](_EXCEPTION_POINTERS* ptr) -> LONG
	{
		auto code = ptr->ExceptionRecord->ExceptionCode;
		switch (code)
		{
		case EXCEPTION_ACCESS_VIOLATION:
			__Birdee_Throw((BirdeeRTTIObject*)birdee_0____create__basic__exception__no__call(0));
			return EXCEPTION_CONTINUE_EXECUTION;
			break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			__Birdee_Throw((BirdeeRTTIObject*)birdee_0____create__basic__exception__no__call(1));
			return EXCEPTION_CONTINUE_EXECUTION;
			break;
		}
		return EXCEPTION_EXECUTE_HANDLER;
	});
}
#endif

extern "C" DLLEXPORT void* BirdeeMallocObj(uint32_t sz, GC_finalization_proc proc)
{
	void* ret= GC_malloc(sz);
	if(proc)
		GC_register_finalizer_no_order(ret, proc, nullptr, nullptr, nullptr);
	return ret;
}

struct ModuleConstructor
{
	ModuleConstructor()
	{
		Init();
	}
};

static ModuleConstructor ctor;

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

extern "C" DLLEXPORT void* BirdeeMallocArr(uint32_t sz,uint32_t param_sz,...)
{
	va_list args;
	va_start(args, param_sz);
	auto ret = BirdeeAllocArr(args, sz,1, param_sz);
	va_end(args);
	return ret;
}

extern "C" DLLEXPORT void prints(char* i)
{
	fputs(i, stdout);
}


extern "C" DLLEXPORT BirdeeString* BirdeeP2S(void* i)
{
	BirdeeString* ret = (BirdeeString*)BirdeeMallocObj(sizeof(BirdeeString),nullptr);
	int sz = snprintf(nullptr, 0, "%p", i) + 1;
	ret->arr = (GenericArray*)BirdeeMallocArr(1, 1, sz);
	ret->sz = sz - 1;
	int retp = snprintf(ret->arr->packed.cbuf, sz, "%p", i);
	assert(retp > 0 && retp < sz);
	return ret;
}

extern "C" DLLEXPORT BirdeeString* BirdeeD2S(double i)
{
	BirdeeString* ret = (BirdeeString*)BirdeeMallocObj(sizeof(BirdeeString), nullptr);
	int sz = snprintf(nullptr, 0, "%f", i) + 1;
	ret->arr = (GenericArray*)BirdeeMallocArr(1, 1, sz);
	ret->sz = sz - 1;
	int retp = snprintf(ret->arr->packed.cbuf, sz, "%f", i);
	assert(retp > 0 && retp < sz);
	return ret;
}

extern "C" DLLEXPORT BirdeeString* BirdeeI2S(int i)
{
	BirdeeString* ret=(BirdeeString*)BirdeeMallocObj(sizeof(BirdeeString), nullptr);
	int sz = snprintf(nullptr, 0, "%i", i) + 1;
	ret->arr = (GenericArray*)BirdeeMallocArr(1, 1, sz);
	ret->sz = sz - 1;
	int retp = snprintf(ret->arr->packed.cbuf, sz, "%i", i);
	assert(retp > 0 && retp < sz);
	return ret;
}
