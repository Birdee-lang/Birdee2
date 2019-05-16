/*
Part of the Birdee Project, under the Apache License v2.0
Modified from LLVM project's ExceptionDemo.cpp
*/
//
//===-- ExceptionDemo.cpp - An example using llvm Exceptions --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifdef _MSC_VER
#include "unwind_windows.h"
#else
#include "unwind.h"
#endif

#include <cstdint>
#include <cassert>
#include "llvm/BinaryFormat/Dwarf.h"
#include <stdio.h>
#include <stddef.h>
#include <gc.h>
#include "BirdeeRuntime.h"
#include <string.h>
#include <stdlib.h>
#include <exception>

//#define EXCEPTION_DEBUG 1

#define MKINT(a,i) (((uint64_t)(a))<<((i)*8))
static const uint64_t MY_EXCEPTION_CLASS = MKINT('M', 7) | MKINT('N', 6) | MKINT('K', 5) // \0
| MKINT('B', 3) | MKINT('R', 2) | MKINT('D', 1); //\0

#ifdef _WIN32
//__builtin_eh_return_data_regno(x) seems always equals to x on Windows
#define  __builtin_eh_return_data_regno(A) (A)
#endif

extern "C" bool birdee_0type__info_0is__parent__of(BirdeeTypeInfo* parent, BirdeeTypeInfo* child);

struct EmptyExceptionStruct
{
	_Unwind_Exception exp;
	BirdeeRTTIObject* pobj;
};

namespace {
	template <typename Type_>
	uintptr_t ReadType(const uint8_t *&p) {
		Type_ value;
		memcpy(&value, p, sizeof(Type_));
		p += sizeof(Type_);
		return static_cast<uintptr_t>(value);
	}
}

// Note: using an extern "C" block so that static functions can be used
extern "C" {


	/// This function is the struct _Unwind_Exception API mandated delete function
	/// used by foreign exception handlers when deleting our exception
	/// (OurException), instances.
	/// @param reason See @link http://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html
	/// @unlink
	/// @param expToDelete exception instance to delete
	void __Birdee_DeleteFromUnwindOurException(_Unwind_Reason_Code reason,
		_Unwind_Exception *expToDelete) {
#ifdef EXCEPTION_DEBUG
		fprintf(stderr,
			"deleteFromUnwindOurException(...).\n");
#endif
		if (expToDelete &&
			(expToDelete->exception_class == MY_EXCEPTION_CLASS)) {
			//todo: Do we need free it by hand?
		}
	}

	void* BirdeeMallocObj(uint32_t sz, GC_finalization_proc proc);

	/// Creates (allocates on the heap), an exception (OurException instance),
	/// of the supplied type info type.
	/// @param type type info type
	static _Unwind_Exception* __Birdee_CreateException(BirdeeRTTIObject* pobj) {
		size_t size = sizeof(EmptyExceptionStruct);
		EmptyExceptionStruct *ret = (EmptyExceptionStruct*)GC_malloc(size);
		ret->exp.exception_class = MY_EXCEPTION_CLASS;
		ret->exp.exception_cleanup = __Birdee_DeleteFromUnwindOurException;
		ret->pobj = pobj;
		return &ret->exp;
	}


	DLLEXPORT BirdeeRTTIObject* __Birdee_BeginCatch(EmptyExceptionStruct* exp)
	{
		return exp->pobj;
	}

	DLLEXPORT void __Birdee_Throw(BirdeeRTTIObject* obj) {
		_Unwind_RaiseException(__Birdee_CreateException(obj));
		fprintf(stderr, "Uncaught exception found! %s\n", obj->type->name->arr->packed.cbuf);
		std::terminate();
		return;
	}

	/// Read a uleb128 encoded value and advance pointer
	/// See Variable Length Data in:
	/// @link http://dwarfstd.org/Dwarf3.pdf @unlink
	/// @param data reference variable holding memory pointer to decode from
	/// @returns decoded value
	static uintptr_t readULEB128(const uint8_t **data) {
		uintptr_t result = 0;
		uintptr_t shift = 0;
		unsigned char byte;
		const uint8_t *p = *data;

		do {
			byte = *p++;
			result |= (byte & 0x7f) << shift;
			shift += 7;
		} while (byte & 0x80);

		*data = p;

		return result;
	}


	/// Read a sleb128 encoded value and advance pointer
	/// See Variable Length Data in:
	/// @link http://dwarfstd.org/Dwarf3.pdf @unlink
	/// @param data reference variable holding memory pointer to decode from
	/// @returns decoded value
	static uintptr_t readSLEB128(const uint8_t **data) {
		uintptr_t result = 0;
		uintptr_t shift = 0;
		unsigned char byte;
		const uint8_t *p = *data;
		do {
			byte = *p++;
			result |= (byte & 0x7f) << shift;
			shift += 7;
		} while (byte & 0x80);

		*data = p;

		if ((byte & 0x40) && (shift < (sizeof(result) << 3))) {
			result |= (~0 << shift);
		}

		return result;
	}

	unsigned getEncodingSize(uint8_t Encoding) {
		if (Encoding == llvm::dwarf::DW_EH_PE_omit)
			return 0;

		switch (Encoding & 0x0F) {
		case llvm::dwarf::DW_EH_PE_absptr:
			return sizeof(uintptr_t);
		case llvm::dwarf::DW_EH_PE_udata2:
			return sizeof(uint16_t);
		case llvm::dwarf::DW_EH_PE_udata4:
			return sizeof(uint32_t);
		case llvm::dwarf::DW_EH_PE_udata8:
			return sizeof(uint64_t);
		case llvm::dwarf::DW_EH_PE_sdata2:
			return sizeof(int16_t);
		case llvm::dwarf::DW_EH_PE_sdata4:
			return sizeof(int32_t);
		case llvm::dwarf::DW_EH_PE_sdata8:
			return sizeof(int64_t);
		default:
			// not supported
			abort();
		}
	}

	/// Read a pointer encoded value and advance pointer
	/// See Variable Length Data in:
	/// @link http://dwarfstd.org/Dwarf3.pdf @unlink
	/// @param data reference variable holding memory pointer to decode from
	/// @param encoding dwarf encoding type
	/// @returns decoded value
	static uintptr_t readEncodedPointer(const uint8_t **data, uint8_t encoding) {
		uintptr_t result = 0;
		const uint8_t *p = *data;

		if (encoding == llvm::dwarf::DW_EH_PE_omit)
			return(result);

		// first get value
		switch (encoding & 0x0F) {
		case llvm::dwarf::DW_EH_PE_absptr:
			result = ReadType<uintptr_t>(p);
			break;
		case llvm::dwarf::DW_EH_PE_uleb128:
			result = readULEB128(&p);
			break;
			// Note: This case has not been tested
		case llvm::dwarf::DW_EH_PE_sleb128:
			result = readSLEB128(&p);
			break;
		case llvm::dwarf::DW_EH_PE_udata2:
			result = ReadType<uint16_t>(p);
			break;
		case llvm::dwarf::DW_EH_PE_udata4:
			result = ReadType<uint32_t>(p);
			break;
		case llvm::dwarf::DW_EH_PE_udata8:
			result = ReadType<uint64_t>(p);
			break;
		case llvm::dwarf::DW_EH_PE_sdata2:
			result = ReadType<int16_t>(p);
			break;
		case llvm::dwarf::DW_EH_PE_sdata4:
			result = ReadType<int32_t>(p);
			break;
		case llvm::dwarf::DW_EH_PE_sdata8:
			result = ReadType<int64_t>(p);
			break;
		default:
			// not supported
			abort();
			break;
		}

		// then add relative offset
		switch (encoding & 0x70) {
		case llvm::dwarf::DW_EH_PE_absptr:
			// do nothing
			break;
		case llvm::dwarf::DW_EH_PE_pcrel:
			result += (uintptr_t)(*data);
			break;
		case llvm::dwarf::DW_EH_PE_textrel:
		case llvm::dwarf::DW_EH_PE_datarel:
		case llvm::dwarf::DW_EH_PE_funcrel:
		case llvm::dwarf::DW_EH_PE_aligned:
		default:
			// not supported
			abort();
			break;
		}

		// then apply indirection
		if (encoding & llvm::dwarf::DW_EH_PE_indirect) {
			result = *((uintptr_t*)result);
		}

		*data = p;

		return result;
	}


	/// Deals with Dwarf actions matching our type infos
	/// (OurExceptionType_t instances). Returns whether or not a dwarf emitted
	/// action matches the supplied exception type. If such a match succeeds,
	/// the resultAction argument will be set with > 0 index value. Only
	/// corresponding llvm.eh.selector type info arguments, cleanup arguments
	/// are supported. Filters are not supported.
	/// See Variable Length Data in:
	/// @link http://dwarfstd.org/Dwarf3.pdf @unlink
	/// Also see @link http://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html @unlink
	/// @param resultAction reference variable which will be set with result
	/// @param classInfo our array of type info pointers (to globals)
	/// @param actionEntry index into above type info array or 0 (clean up).
	///        We do not support filters.
	/// @param exceptionClass exception class (_Unwind_Exception::exception_class)
	///        of thrown exception.
	/// @param exceptionObject thrown _Unwind_Exception instance.
	/// @returns whether or not a type info was found. False is returned if only
	///          a cleanup was found
	static bool handleActionValue(int64_t *resultAction,
		uint8_t TTypeEncoding,
		const uint8_t *ClassInfo,
		uintptr_t actionEntry,
		uint64_t exceptionClass,
		struct _Unwind_Exception *exceptionObject) {
		bool ret = false;

		if (!resultAction ||
			!exceptionObject ||
			(exceptionClass != MY_EXCEPTION_CLASS))
			return(ret);
		EmptyExceptionStruct* expptr = (EmptyExceptionStruct*)exceptionObject;
		BirdeeTypeInfo* tyinfo = expptr->pobj->type;

#ifdef EXCEPTION_DEBUG
		fprintf(stderr,
			"handleActionValue(...): exceptionObject = <%p>, "
			"tyinfo = <%s>.\n",
			(void*)exceptionObject,
			tyinfo->name->arr->packed.cbuf);
#endif

		const uint8_t *actionPos = (uint8_t*)actionEntry,
			*tempActionPos;
		int64_t typeOffset = 0,
			actionOffset;

		for (int i = 0; true; ++i) {
			// Each emitted dwarf action corresponds to a 2 tuple of
			// type info address offset, and action offset to the next
			// emitted action.
			typeOffset = readSLEB128(&actionPos);
			tempActionPos = actionPos;
			actionOffset = readSLEB128(&tempActionPos);

#ifdef EXCEPTION_DEBUG
			fprintf(stderr,
				"handleActionValue(...):typeOffset: <%" PRIi64 ">, "
				"actionOffset: <%" PRIi64 ">.\n",
				typeOffset,
				actionOffset);
#endif
			assert((typeOffset >= 0) &&
				"handleActionValue(...):filters are not supported.");

			// Note: A typeOffset == 0 implies that a cleanup llvm.eh.selector
			//       argument has been matched.
			if (typeOffset > 0) {
				unsigned EncSize = getEncodingSize(TTypeEncoding);
				const uint8_t *EntryP = ClassInfo - typeOffset * EncSize;
				uintptr_t P = readEncodedPointer(&EntryP, TTypeEncoding);
				BirdeeTypeInfo *ThisClassInfo =
					reinterpret_cast<BirdeeTypeInfo *>(P);
#ifdef EXCEPTION_DEBUG
				fprintf(stderr,
					"handleActionValue(...):actionValue <%d> <%s> found.\n",
					i, ThisClassInfo->name->arr->packed.cbuf);
#endif
				if (birdee_0type__info_0is__parent__of(ThisClassInfo, tyinfo)) {
					*resultAction = typeOffset;
					ret = true;
					break;
				}
			}

#ifdef EXCEPTION_DEBUG
			fprintf(stderr,
				"handleActionValue(...):actionValue not found.\n");
#endif
			if (!actionOffset)
				break;

			actionPos += actionOffset;
		}

		return(ret);
	}


	/// Deals with the Language specific data portion of the emitted dwarf code.
	/// See @link http://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html @unlink
	/// @param version unsupported (ignored), unwind version
	/// @param lsda language specific data area
	/// @param _Unwind_Action actions minimally supported unwind stage
	///        (forced specifically not supported)
	/// @param exceptionClass exception class (_Unwind_Exception::exception_class)
	///        of thrown exception.
	/// @param exceptionObject thrown _Unwind_Exception instance.
	/// @param context unwind system context
	/// @returns minimally supported unwinding control indicator
	static _Unwind_Reason_Code handleLsda(int version, const uint8_t *lsda,
		_Unwind_Action actions,
		_Unwind_Exception_Class exceptionClass,
		struct _Unwind_Exception *exceptionObject,
		struct _Unwind_Context *context) {
		_Unwind_Reason_Code ret = _URC_CONTINUE_UNWIND;

		if (!lsda)
			return(ret);

#ifdef EXCEPTION_DEBUG
		fprintf(stderr,
			"handleLsda(...):lsda is non-zero.\n");
#endif

		// Get the current instruction pointer and offset it before next
		// instruction in the current frame which threw the exception.
		uintptr_t pc = _Unwind_GetIP(context) - 1;

		// Get beginning current frame's code (as defined by the
		// emitted dwarf code)
		uintptr_t funcStart = _Unwind_GetRegionStart(context);
		uintptr_t pcOffset = pc - funcStart;
		const uint8_t *ClassInfo = NULL;

		// Note: See JITDwarfEmitter::EmitExceptionTable(...) for corresponding
		//       dwarf emission

		// Parse LSDA header.
		uint8_t lpStartEncoding = *lsda++;

		if (lpStartEncoding != llvm::dwarf::DW_EH_PE_omit) {
			readEncodedPointer(&lsda, lpStartEncoding);
		}

		uint8_t ttypeEncoding = *lsda++;
		uintptr_t classInfoOffset;

		if (ttypeEncoding != llvm::dwarf::DW_EH_PE_omit) {
			// Calculate type info locations in emitted dwarf code which
			// were flagged by type info arguments to llvm.eh.selector
			// intrinsic
			classInfoOffset = readULEB128(&lsda);
			ClassInfo = lsda + classInfoOffset;
		}

		// Walk call-site table looking for range that
		// includes current PC.

		uint8_t         callSiteEncoding = *lsda++;
		uint32_t        callSiteTableLength = readULEB128(&lsda);
		const uint8_t   *callSiteTableStart = lsda;
		const uint8_t   *callSiteTableEnd = callSiteTableStart +
			callSiteTableLength;
		const uint8_t   *actionTableStart = callSiteTableEnd;
		const uint8_t   *callSitePtr = callSiteTableStart;

		while (callSitePtr < callSiteTableEnd) {
			uintptr_t start = readEncodedPointer(&callSitePtr,
				callSiteEncoding);
			uintptr_t length = readEncodedPointer(&callSitePtr,
				callSiteEncoding);
			uintptr_t landingPad = readEncodedPointer(&callSitePtr,
				callSiteEncoding);

			// Note: Action value
			uintptr_t actionEntry = readULEB128(&callSitePtr);

			if (exceptionClass != MY_EXCEPTION_CLASS) {
				// We have been notified of a foreign exception being thrown,
				// and we therefore need to execute cleanup landing pads
				actionEntry = 0;
			}

			if (landingPad == 0) {
#ifdef EXCEPTION_DEBUG
				fprintf(stderr,
					"handleLsda(...): No landing pad found.\n");
#endif

				continue; // no landing pad for this entry
			}

			if (actionEntry) {
				actionEntry += ((uintptr_t)actionTableStart) - 1;
			}
			else {
#ifdef EXCEPTION_DEBUG
				fprintf(stderr,
					"handleLsda(...):No action table found.\n");
#endif
			}

			bool exceptionMatched = false;

			if ((start <= pcOffset) && (pcOffset < (start + length))) {
#ifdef EXCEPTION_DEBUG
				fprintf(stderr,
					"handleLsda(...): Landing pad found.\n");
#endif
				int64_t actionValue = 0;

				if (actionEntry) {
					exceptionMatched = handleActionValue(&actionValue,
						ttypeEncoding,
						ClassInfo,
						actionEntry,
						exceptionClass,
						exceptionObject);
				}

				if (!(actions & _UA_SEARCH_PHASE)) {
					if (exceptionMatched)
					{
#ifdef EXCEPTION_DEBUG
						fprintf(stderr,
							"handleLsda(...): installed landing pad "
							"context.\n");
#endif

						// Found landing pad for the PC.
						// Set Instruction Pointer to so we re-enter function
						// at landing pad. The landing pad is created by the
						// compiler to take two parameters in registers.
						_Unwind_SetGR(context,
							__builtin_eh_return_data_regno(0),
							(uintptr_t)exceptionObject);

						// Note: this virtual register directly corresponds
						//       to the return of the llvm.eh.selector intrinsic
						if (!actionEntry || !exceptionMatched) {
							// We indicate cleanup only
							_Unwind_SetGR(context,
								__builtin_eh_return_data_regno(1),
								0);
						}
						else {
							// Matched type info index of llvm.eh.selector intrinsic
							// passed here.
							_Unwind_SetGR(context,
								__builtin_eh_return_data_regno(1),
								actionValue);
						}

						// To execute landing pad set here
						_Unwind_SetIP(context, funcStart + landingPad);
						ret = _URC_INSTALL_CONTEXT;
					}
				}
				else if (exceptionMatched) {
#ifdef EXCEPTION_DEBUG
					fprintf(stderr,
						"handleLsda(...): setting handler found.\n");
#endif
					ret = _URC_HANDLER_FOUND;
				}
				else {
					// Note: Only non-clean up handlers are marked as
					//       found. Otherwise the clean up handlers will be
					//       re-found and executed during the clean up
					//       phase.
#ifdef EXCEPTION_DEBUG
					fprintf(stderr,
						"handleLsda(...): cleanup handler found.\n");
#endif
				}

				break;
			}
		}

		return(ret);
	}

	/// This is the personality function which is embedded (dwarf emitted), in the
	/// dwarf unwind info block. Again see: JITDwarfEmitter.cpp.
	/// See @link http://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html @unlink
	/// @param version unsupported (ignored), unwind version
	/// @param _Unwind_Action actions minimally supported unwind stage
	///        (forced specifically not supported)
	/// @param exceptionClass exception class (_Unwind_Exception::exception_class)
	///        of thrown exception.
	/// @param exceptionObject thrown _Unwind_Exception instance.
	/// @param context unwind system context
	/// @returns minimally supported unwinding control indicator
	static _Unwind_Reason_Code __Birdee_Personality_impl(int version, _Unwind_Action actions,
		_Unwind_Exception_Class exceptionClass,
		struct _Unwind_Exception *exceptionObject,
		struct _Unwind_Context *context) {
#ifdef EXCEPTION_DEBUG
		fprintf(stderr,
			"We are in ourPersonality(...):actions is <%d>.\n",
			actions);

		if (actions & _UA_SEARCH_PHASE) {
			fprintf(stderr, "ourPersonality(...):In search phase.\n");
		}
		else {
			fprintf(stderr, "ourPersonality(...):In non-search phase.\n");
		}
#endif

		const uint8_t *lsda = (const uint8_t *)_Unwind_GetLanguageSpecificData(context);

#ifdef EXCEPTION_DEBUG
		fprintf(stderr,
			"ourPersonality(...):lsda = <%p>.\n",
			(void*)lsda);
#endif

		// The real work of the personality function is captured here
		return(handleLsda(version,
			lsda,
			actions,
			exceptionClass,
			exceptionObject,
			context));
	}

#if defined(_WIN32)
#include <Windows.h>

	extern EXCEPTION_DISPOSITION _GCC_specific_handler(PEXCEPTION_RECORD, void *,
		PCONTEXT, PDISPATCHER_CONTEXT,
		_Unwind_Personality_Fn);

	DLLEXPORT EXCEPTION_DISPOSITION
		__Birdee_Personality(PEXCEPTION_RECORD ms_exc, void *this_frame,
			PCONTEXT ms_orig_context, PDISPATCHER_CONTEXT ms_disp)
	{
		return _GCC_specific_handler(ms_exc, this_frame, ms_orig_context, ms_disp,
			__Birdee_Personality_impl);
	}
#else
	_Unwind_Reason_Code __Birdee_Personality(int version, _Unwind_Action actions,
		_Unwind_Exception_Class exceptionClass,
		struct _Unwind_Exception *exceptionObject,
		struct _Unwind_Context *context)
	{
		return __Birdee_Personality_impl(version, actions, exceptionClass, exceptionObject, context);
	}
#endif
}