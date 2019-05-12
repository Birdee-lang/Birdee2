//===- KaleidoscopeJIT.h - A simple JIT for Kaleidoscope --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Contains a simple JIT definition for use in the kaleidoscope tutorials.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H
#define LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H

#include "llvm/ADT/iterator_range.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/LambdaResolver.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <LibDef.h>
#include <unordered_map>

namespace llvm {
	namespace orc {

		class KaleidoscopeJIT {
		public:
			BD_CORE_API static std::unique_ptr<KaleidoscopeJIT> Create();
			using ObjLayerT = RTDyldObjectLinkingLayer;
			using CompileLayerT = IRCompileLayer<ObjLayerT, SimpleCompiler>;
			using ModuleHandleT = CompileLayerT::ModuleHandleT;

			BD_CORE_API KaleidoscopeJIT();
			std::unordered_map<std::string, void*> natives;
			BD_CORE_API TargetMachine &getTargetMachine();

			BD_CORE_API void setTargetMachine(Module* m);
			BD_CORE_API ModuleHandleT addModule(std::unique_ptr<Module> M);
			BD_CORE_API void addNative(const char* name, void* pfunc);
			BD_CORE_API void removeModule(ModuleHandleT H);

			BD_CORE_API JITSymbol findSymbol(const std::string Name);
			BD_CORE_API ~KaleidoscopeJIT();
		private:
			std::string mangle(const std::string &Name);

			JITSymbol findMangledSymbol(const std::string &Name);

			std::unique_ptr<TargetMachine> TM;
			const DataLayout DL;
			ObjLayerT ObjectLayer;
			CompileLayerT CompileLayer;
			std::vector<ModuleHandleT> ModuleHandles;
		};

	} // end namespace orc
} // end namespace llvm

#endif // LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H
