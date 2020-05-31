#pragma once
#include "LibDef.h"
namespace Birdee
{
	class BD_CORE_API PyHandle
	{
		void* p;
	public:
		void* ptr();
		~PyHandle();
		void reset();
		void set(void* newp);
		PyHandle();
		PyHandle(void* ptr);
		PyHandle(PyHandle&& other) noexcept;
		//PyHandle& operator =(PyHandle&& other);
	};
}