
#include "Tokenizer.h"
#include "BdAST.h"
#include "CompileError.h"
#include <fstream>
#include <cstdlib>
#include <fstream>
using namespace Birdee;

extern int ParseTopLevel();
extern void SeralizeMetadata(std::ostream& out);

extern void RunGenerativeScript();
BD_CORE_API extern Tokenizer tokenizer;


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
				cu.expose_main = true;
			}
			else if (cmd == "-p")
			{
				if (!args.HasNext())
					goto fail;
				string ret = args.Get();
				cu.symbol_prefix = ret;
			}
			else if (cmd == "--script" || cmd== "-s")
			{
				cu.is_script_mode = true;
			}
			else if (cmd == "--printir")
			{
				cu.is_print_ir = true;
			}
			else if (cmd == "--corelib")
			{
				cu.is_corelib = true;
			}
			else
				goto fail;
		}
		if ( source == "" || target == "")
			goto fail;
		//cut the source path into filename & dir path
		size_t found;
		found = source.find_last_of("/\\");
		if (found == string::npos)
		{
			cu.directory = ".";
			cu.filename = source;
		}
		else
		{
			cu.directory = source.substr(0, found);
			cu.filename = source.substr(found + 1);
		}
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
		found = target.find_last_of('.');
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
	

	if (cu.is_script_mode)
	{
		RunGenerativeScript();
		return 0;
	}

	cu.InitForGenerate();
	try {
		ParseTopLevel();
		cu.Phase0();
		cu.Phase1();
		cu.Generate();
	}
	catch (CompileError e)
	{
		e.print();
		return 1;
	}
	catch (TokenizerError e)
	{
		e.print();
		return 1;
	}
	/*for (auto&& node : cu.toplevel)
	{
		node->print(0);
	}*/
	std::ofstream metaf(cu.targetmetapath, std::ios::out);
	SeralizeMetadata(metaf);
    return 0;
}

