
#include "Tokenizer.h"
#include "BdAST.h"
#include "CompileError.h"
#include <fstream>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <CompilerOptions.h>
using namespace Birdee;

extern int ParseTopLevel(bool autoimport=true);
extern void SeralizeMetadata(std::ostream& out, bool is_empty);
extern void ParseTopLevelImportsOnly();
extern string GetModuleNameByArray(const vector<string>& package, const char* delimiter = ".");

//GetCurrentDir copied from http://www.codebind.com/cpp-tutorial/c-get-current-directory-linuxwindows/
#include <stdio.h>  /* defines FILENAME_MAX */
// #define WINDOWS  /* uncomment this line to use it for windows.*/ 

namespace Birdee
{
	extern BD_CORE_API void SetSourceFilePath(const string& source);
}

#ifdef _WIN32
extern int RunGenerativeScript();
#else
#include <dlfcn.h>
extern void* LoadBindingFunction(const char* name);
	static int RunGenerativeScript()
	{
		typedef void(*PtrImpl)();
		static PtrImpl impl = nullptr;
		if (impl == nullptr)
		{   
			impl = (PtrImpl)LoadBindingFunction( "_Z19RunGenerativeScriptv");
		}
		impl();
	}
#endif
BD_CORE_API extern Tokenizer tokenizer;

static bool is_print_import_mode = false;

struct ArgumentsStream
{
	int argc;
	char** argv;
	int current = 1;
	ArgumentsStream(int argc, char** argv): argc(argc),argv(argv){}
	string Get()
	{
		if (current >= argc)
			return "";
		return argv[current++];
	}
	bool HasNext()
	{
		return argc > current;
	}
	string Peek()
	{
		if (current >= argc)
			return "";
		return argv[current];
	}
	void Consume() { current++; }
};



void ParseParameters(int argc, char** argv)
{
	ArgumentsStream args(argc, argv);
	string source, target;
	{
		while (args.HasNext())
		{
			string cmd = args.Get();
			if (cmd == "-i")
			{
				if (!args.HasNext())
					goto fail;
				source = args.Get();
			}
			else if (cmd == "-o")
			{
				if (!args.HasNext())
					goto fail;
				target = args.Get();
			}
			else if (cmd == "-e")
			{
				cu.options->expose_main = true;
			}
			else if (cmd == "-p")
			{
				if (!args.HasNext())
					goto fail;
				string ret = args.Get();
				cu.symbol_prefix = ret;
			}
			else if (cmd == "--print-import")
			{
				is_print_import_mode = true;
			}
			else if (cmd == "--script" || cmd== "-s")
			{
				cu.is_script_mode = true;
			}
			else if (cmd == "--printir")
			{
				cu.options->is_print_ir = true;
			}
			else if (cmd == "--corelib")
			{
				cu.is_corelib = true;
			}
			else if (cmd=="-l") //L not 1
			{
				if (!args.HasNext())
					goto fail;
				string str = args.Get();
				if (str.back() != '\\' && str.back() != '/')
					str.push_back('/');
				cu.search_path.push_back(std::move(str));
			}
			else if (cmd == "-O" || cmd == "--optimize")
			{
				if (!args.HasNext())
					goto fail;
				string str = args.Get();
				int opt = std::stoi(str);
				if (opt < 0 || opt>4)
				{
					std::cerr << "Bad optimization level. Should be within 0 to 3\n";
					goto fail;
				}
				cu.options->optimize_level = opt;
			}
			else if (cmd == "--llvm-opt-start")
			{
				for (;;)
				{
					if (!args.HasNext())
					{
						std::cerr << "No --llvm-opt-end matches --llvm-opt-start\n";
						goto fail;
					}
					string str = args.Get();
					if (str == "--llvm-opt-end")
						break;
					cu.options->llvm_options.push_back(std::move(str));
				}
			}
			else if (cmd == "-Z" || cmd == "--size-optimize")
			{
				if (!args.HasNext())
					goto fail;
				string str = args.Get();
				int zopt = std::stoi(str);
				if (zopt < 0 || zopt>3)
				{
					std::cerr << "Bad optimization level. Should be within 0 to 2\n";
					goto fail;
				}
				cu.options->size_optimize_level = zopt;
			}
			else
				goto fail;
		}
		if (source == "")
		{
			std::cerr << "The input file is not specified.\n";
			goto fail;
		}
		if (target == "" && !is_print_import_mode)
		{
			std::cerr << "The output file is not specified.\n";
			goto fail;
		}
		//cut the source path into filename & dir path
		SetSourceFilePath(source);
		if (!cu.is_script_mode)
		{
			auto f = std::make_unique<FileStream>(source.c_str());
			if (!f->Okay())
			{
				std::cerr << "Error when opening file " << source << "\n";
				exit(3);
			}
			Birdee::source_paths.push_back(source);
			tokenizer = Tokenizer(std::move(f), 0);
		}
		auto found = target.find_last_of('.');
		if (found != string::npos)
		{
			cu.targetmetapath = target.substr(0, found) + ".bmm";
		}
		else
		{
			cu.targetmetapath = target + ".bmm";
		}
		cu.targetpath = target;
		return;
	}
fail:
	std::cerr << "Error in the arguments\n";
	exit(2);
}

int main(int argc,char** argv)
{
	cu.is_compiler_mode = true;
	ParseParameters(argc, argv);
	
	//it is important to clear the imported modules before python interpreter quits
	//because imported modules may have PyObject reference, and they must be deleted before
	//destruction of python interpreter 

	if (cu.is_script_mode)
	{
		return RunGenerativeScript();
	}
	int is_empty = false;
	try {
		if (is_print_import_mode)
		{
			ParseTopLevelImportsOnly();
			for (auto& imp : cu.imports)
			{
				std::cout<<GetModuleNameByArray(imp)<<'\n';
			}
			std::cout << cu.symbol_prefix;
			return 0;
		}
		else
		{
			cu.InitForGenerate();
			ParseTopLevel();
			cu.Phase0();
			cu.Phase1();
			is_empty = cu.Generate();
		}
	}
	catch (CompileError& e)
	{
		e.print();
		return 1;
	}
	catch (TokenizerError& e)
	{
		e.print();
		return 1;
	}
	/*for (auto&& node : cu.toplevel)
	{
		node->print(0);
	}*/
	std::ofstream metaf(cu.targetmetapath, std::ios::out);
	SeralizeMetadata(metaf, is_empty);
    return 0;
}

