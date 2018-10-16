#pragma once

#if defined(_WIN32) && defined(BIRDEE_USE_DYN_LIB)
	#ifdef BIRDEE_CORE_LIB
		#define BD_CORE_API __declspec(dllexport)
	#else
		#define BD_CORE_API __declspec(dllimport)
	#endif
	#define BIRDEE_BINDING_API __declspec(dllexport)
	
#else
	#if defined(BIRDEE_USE_DYN_LIB) && !defined(_WIN32)
		#define BD_CORE_API __attribute__((visibility("default")))
		#define BIRDEE_BINDING_API __attribute__((visibility("default")))
	#endif
#endif

