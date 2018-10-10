#pragma once

#if defined(_WIN32) && defined(BIRDEE_USE_DYN_LIB)
	#ifdef BIRDEE_CORE_LIB
		#define BD_CORE_API __declspec(dllexport)
	#else
		#define BD_CORE_API __declspec(dllimport)
	#endif
	#define BIRDEE_BINDING_API __declspec(dllexport)
	
#else
	#define BD_CORE_API
	#define BIRDEE_BINDING_API
#endif

