
#ifndef _MSC_VER
extern "C" int DLLMAIN();
__attribute__((constructor)) static void __constructor();

static void __constructor() {
	DLLMAIN();
}
#else

#endif

