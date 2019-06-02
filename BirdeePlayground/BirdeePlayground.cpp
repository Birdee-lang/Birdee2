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

#ifdef _WIN32
#include "BirdeeIncludes.h"
#endif

using namespace Birdee;
extern int ParseTopLevel(bool autoimport);
BD_CORE_API extern Tokenizer tokenizer;
BD_CORE_API llvm::TargetMachine* GetAndSetTargetMachine();
BD_CORE_API extern llvm::Module* GetLLVMModuleRef();
BD_CORE_API extern unique_ptr<llvm::Module> GetLLVMModule();
BD_CORE_API extern void AddAutoImport();
BD_CORE_API extern void DoImportName(const vector<string>& package, const string& name);
BD_CORE_API extern ImportedModule* DoImportPackage(const vector<string>& package);
BD_CORE_API extern void RollbackTemplateInstances();

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

template<typename Derived>
Derived* dyncast_resolve_anno(StatementAST* p)
{
	if (typeid(*p) == typeid(AnnotationStatementAST)) {
		return dyncast_resolve_anno<Derived>( static_cast<AnnotationStatementAST*>(p)->impl.get() );
	}
	if (typeid(*p) == typeid(Derived)) {
		return static_cast<Derived*>(p);
	}
	return nullptr;
}

int main()
{
	cu.is_compiler_mode = true;
	cu.name = "birdeerepl";
	cu.InitForGenerate();

	AddAutoImport();
	DoImportName({ "fmt" }, "printany");
	auto pkgtree = cu.imported_packages.FindName("fmt");
	if (!pkgtree || !pkgtree->mod)
	{
		std::cerr << "Cannot import package fmt\n";
		std::exit(2);
	}
	auto printany_func_itr = pkgtree->mod->funcmap.find("printany");
	if (printany_func_itr == pkgtree->mod->funcmap.end())
	{
		std::cerr << "Cannot import function fmt.printany\n";
		std::exit(2);
	}
	auto printany_func = printany_func_itr->second.first.get();

	std::unique_ptr < llvm::orc::KaleidoscopeJIT> TheJIT = make_unique<llvm::orc::KaleidoscopeJIT>();
#ifdef _WIN32
	AddFunctions(TheJIT.get());
#endif
	//cu.options->is_print_ir = true;
	cu.symbol_prefix = "__repl.";
	std::string str_printir = ":printir=";
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
		std::cout << "birdee>> ";
		std::string str;
		std::getline(std::cin, str);
		if (str == ":q")
			break;
		if (str.substr(0, str_printir.size()) == str_printir)
		{
			cu.options->is_print_ir = str.substr(str_printir.size()) == "true";
			continue;
		}

		auto f = std::make_unique<StdinStream>();
		auto mystream = f.get();
		tokenizer = Tokenizer(std::move(f), 0);
		str += '\n'; str += '\n'; str += TOKENIZER_PENDING_CHAR;
		mystream->buf = std::move(str);
		
		try
		{
			auto m = GetLLVMModuleRef();
			TheJIT->setTargetMachine(m);
			ParseTopLevel(false);
			cu.Phase0();
			cu.Phase1();

			std::string prompt;
			if (!cu.toplevel.empty())
			{
				auto stmt = cu.toplevel.back().get();
				if (auto expr = dynamic_cast<ExprAST*>(stmt))
				{
					if(auto funcast = dyncast_resolve_anno<FunctionAST>(expr))
					{
						prompt = expr->resolved_type.GetString();
						prompt += ": Function ";
						prompt += funcast->Proto->GetName();
					}
					else if (expr->resolved_type.isResolved() && expr->resolved_type.type != tok_void)
					{
						//if the expression has a value, make a call to fmt.printast
						//first, get an instance of printast[T]
						auto targs = make_unique<vector<TemplateArgument>>();
						targs->push_back(TemplateArgument(expr->resolved_type));
						auto the_func = printany_func->template_param->GetOrCreate(std::move(targs), printany_func, stmt->Pos);
						
						//prepare the arguments
						std::vector<std::unique_ptr<ExprAST>> arg;
						arg.emplace_back(unique_ptr_cast<ExprAST>(std::move(cu.toplevel.back())));
						
						//generate a call to printany[T]
						cu.toplevel.back() = std::make_unique<CallExprAST>(
							std::make_unique<ResolvedFuncExprAST>(the_func, tokenizer.GetSourcePos()),
							std::move(arg)
							);

						prompt = expr->resolved_type.GetString();
						prompt += ": ";
					}
				}
			}

			cu.GenerateIR(true, false);

			TheJIT->addModule(GetLLVMModule());
			auto func = TheJIT->findSymbol("__anoy_func_main");
			assert(func && "Function not found");
			void(*pfunc)() = (void(*)())func.getAddress().get();

			if (!prompt.empty())
				std::cout << prompt;
			pfunc();
			if (!prompt.empty())
				std::cout << std::endl;
			cu.SwitchModule();

			cu.ClearParserAndProprocessing();
		}
		catch (CompileError& e)
		{
			e.print();
			std::cout << std::endl;
			RollbackTemplateInstances();
			cu.ClearParserAndProprocessing();
		}
		catch (TokenizerError& e)
		{
			e.print();
			std::cout << std::endl;
			RollbackTemplateInstances();
			cu.ClearParserAndProprocessing();
		}
	}

}
