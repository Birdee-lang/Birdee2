// BirdeePlayground.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <BdAST.h>
#include <CompileError.h>
#include <Tokenizer.h>
#include <LibDef.h>
#include <CompilerOptions.h>
#include <iostream>
#include <Util.h>
#include <KaleidoscopeJIT.h>
#include "BirdeeIncludes.h"

using namespace Birdee;
extern int ParseTopLevel(bool autoimport);
BD_CORE_API extern Tokenizer tokenizer;
BD_CORE_API llvm::TargetMachine* GetAndSetTargetMachine();
BD_CORE_API extern llvm::Module* GetLLVMModuleRef();
BD_CORE_API extern unique_ptr<llvm::Module> GetLLVMModule();

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

//a line-cached stdin stream
class StdinStream : public Birdee::Stream
{
public:
	std::string buf;
	int buf_cnt = 0;
	bool can_eof = false;
	int Getc() override
	{
		if (buf_cnt >= buf.size())
		{
			if (can_eof)
			{
				can_eof = false;
				return EOF;
			}
			if (std::cin.eof())
				std::exit(0);
			std::cout << "........ ";
			std::getline(std::cin, buf);
			buf += '\n'; buf += '\n';
			buf += TOKENIZER_PENDING_CHAR;
			buf_cnt = 0;
			return Getc();
		}
		int ret = buf[buf_cnt];
		buf_cnt++;
		return ret;
	}
	void SetCanEOF() override
	{
		can_eof = true;
	}
};

int main()
{
	cu.is_compiler_mode = true;
	cu.name = "birdeerepl";
	cu.InitForGenerate();
	std::unique_ptr < llvm::orc::KaleidoscopeJIT> TheJIT = make_unique<llvm::orc::KaleidoscopeJIT>();
	AddFunctions(TheJIT.get());
	//cu.options->is_print_ir = true;
	bool is_first = true;
	cu.symbol_prefix = "__repl";
	Birdee::source_paths.push_back("(REPL)");
	std::cout << R"(Birdee Playground
    ____  _          __        
   / __ )(_)________/ /__  ___ 
  / __  / / ___/ __  / _ \/ _ \
 / /_/ / / /  / /_/ /  __/  __/
/_____/_/_/   \__,_/\___/\___/ 
                               
You can try and see Birdee codes here. Type ":q" to exit.
)";
	for (;;)
	{
		auto f = std::make_unique<StdinStream>();
		auto mystream = f.get();
		tokenizer = Tokenizer(std::move(f), 0);
		std::cout << "birdee>> ";
		std::string str;
		std::getline(std::cin, str);
		if (str == ":q")
			break;
		str += '\n'; str += '\n'; str += TOKENIZER_PENDING_CHAR;
		mystream->buf = std::move(str);
		
		try
		{
			auto m = GetLLVMModuleRef();
			TheJIT->setTargetMachine(m);
			ParseTopLevel(is_first);
			is_first = false;
			cu.Phase0();
			cu.Phase1();
			cu.GenerateIR(true, false);
			TheJIT->addModule(GetLLVMModule());
			auto func = TheJIT->findSymbol("__anoy_func_main");
			assert(func && "Function not found");
			void(*pfunc)() = (void(*)())func.getAddress().get();
			pfunc();
			cu.SwitchModule();
			cu.ClearParserAndProprocessing();
		}
		catch (CompileError& e)
		{
			e.print();
			cu.ClearParserAndProprocessing();
		}
		catch (TokenizerError& e)
		{
			e.print();
			cu.ClearParserAndProprocessing();
		}
	}

}
