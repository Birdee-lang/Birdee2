#pragma once
#include "LibDef.h"
namespace Birdee
{
	class PyHandle
	{
		void* p;
	public:
		void* ptr();
		BD_CORE_API ~PyHandle();
		void reset();
		void set(void* newp);
		PyHandle();
		PyHandle(void* ptr);
		PyHandle(PyHandle&& other) noexcept;
		PyHandle& operator =(PyHandle&& other);
	};
}