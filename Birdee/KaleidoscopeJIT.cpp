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

#include <KaleidoscopeJIT.h>

namespace llvm {
	namespace orc {
		std::unique_ptr<KaleidoscopeJIT> KaleidoscopeJIT::Create()
		{
			return std::make_unique<KaleidoscopeJIT>();
		}
		KaleidoscopeJIT::KaleidoscopeJIT()
			: TM(EngineBuilder().selectTarget()), DL(TM->createDataLayout()),
			ObjectLayer([]() { return std::make_shared<SectionMemoryManager>(); }),
			CompileLayer(ObjectLayer, SimpleCompiler(*TM)) {
			llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
		}

		TargetMachine &KaleidoscopeJIT::getTargetMachine() { return *TM; }

		void KaleidoscopeJIT::setTargetMachine(Module * m)
		{
			m->setDataLayout(TM->createDataLayout());
		}

		KaleidoscopeJIT::ModuleHandleT KaleidoscopeJIT::addModule(std::unique_ptr<Module> M) {
			// We need a memory manager to allocate memory and resolve symbols for this
			// new module. Create one that resolves symbols by looking back into the
			// JIT.
			auto Resolver = createLambdaResolver(
				[&](const std::string &Name) {
				auto itr = natives.find(Name);
				if (itr != natives.end())
					return JITSymbol((uint64_t)itr->second, JITSymbolFlags::Exported);
				if (auto Sym = findMangledSymbol(Name))
					return Sym;
				return JITSymbol(nullptr);
			},
				[](const std::string &S) { return nullptr; });
			auto H = cantFail(CompileLayer.addModule(std::move(M),
				std::move(Resolver)));

			ModuleHandles.push_back(H);
			return H;
		}

		void KaleidoscopeJIT::removeModule(ModuleHandleT H) {
			ModuleHandles.erase(find(ModuleHandles, H));
			cantFail(CompileLayer.removeModule(H));
		}

		JITSymbol KaleidoscopeJIT::findSymbol(const std::string Name) {
			return findMangledSymbol(mangle(Name));
		}

		KaleidoscopeJIT::~KaleidoscopeJIT()
		{
		}

		std::string KaleidoscopeJIT::mangle(const std::string &Name) {
			std::string MangledName;
			{
				raw_string_ostream MangledNameStream(MangledName);
				Mangler::getNameWithPrefix(MangledNameStream, Name, DL);
			}
			return MangledName;
		}

		void KaleidoscopeJIT::addNative(const char* name, void* pfunc) {
			natives[name] = pfunc;
		}

		JITSymbol KaleidoscopeJIT::findMangledSymbol(const std::string &Name) {
#ifdef LLVM_ON_WIN32
			// The symbol lookup of ObjectLinkingLayer uses the SymbolRef::SF_Exported
			// flag to decide whether a symbol will be visible or not, when we call
			// IRCompileLayer::findSymbolIn with ExportedSymbolsOnly set to true.
			//
			// But for Windows COFF objects, this flag is currently never set.
			// For a potential solution see: https://reviews.llvm.org/rL258665
			// For now, we allow non-exported symbols on Windows as a workaround.
			const bool ExportedSymbolsOnly = false;
#else
			const bool ExportedSymbolsOnly = true;
#endif

			// Search modules in reverse order: from last added to first added.
			// This is the opposite of the usual search order for dlsym, but makes more
			// sense in a REPL where we want to bind to the newest available definition.
			for (auto H : make_range(ModuleHandles.rbegin(), ModuleHandles.rend()))
				if (auto Sym = CompileLayer.findSymbolIn(H, Name, ExportedSymbolsOnly))
					return Sym;

			// If we can't find the symbol in the JIT, try looking in the host process.
			if (auto SymAddr = RTDyldMemoryManager::getSymbolAddressInProcess(Name))
				return JITSymbol(SymAddr, JITSymbolFlags::Exported);

#ifdef LLVM_ON_WIN32
			// For Windows retry without "_" at beginning, as RTDyldMemoryManager uses
			// GetProcAddress and standard libraries like msvcrt.dll use names
			// with and without "_" (for example "_itoa" but "sin").
			if (Name.length() > 2 && Name[0] == '_')
				if (auto SymAddr =
					RTDyldMemoryManager::getSymbolAddressInProcess(Name.substr(1)))
					return JITSymbol(SymAddr, JITSymbolFlags::Exported);
#endif

			return nullptr;
		}



	} // end namespace orc
} // end namespace llvm

