
#include "Tokenizer.h"
#include "AST.h"
#include "CompileError.h"
#include <fstream>
#include <cstdlib>
using namespace Birdee;

extern int ParseTopLevel();
extern void SeralizeMetadata(std::ostream& out);

namespace Birdee
{
	CompileUnit cu;
}

Tokenizer tokenizer(nullptr);

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
		else if (cmd == "--corelib")
		{
			cu.is_corelib = true;
		}
		else
			goto fail;
	}
	if (source == "" || target == "")
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
	found = target.find_last_of('.');
	if (found != string::npos)
	{
		cu.targetmetapath = target.substr(0,found)+".bmm";
	}
	else
	{
		cu.targetmetapath = target + ".bmm";
	}
	cu.targetpath = target;
	FILE* f;
	f= fopen(source.c_str(), "r");
	if (!f)
	{
		std::cerr << "Error when opening file "<<source<<"\n";
		exit(3);
	}
	tokenizer = Tokenizer(f);
	return;
fail:
	std::cerr << "Error in the arguments\n";
	exit(2);
}

int main(int argc,char** argv)
{
	ParseParameters(argc, argv);
	cu.InitForGenerate();
	char* home = std::getenv("BIRDEE_HOME");
	if (home)
	{
		cu.homepath = home;
		if (cu.homepath.back() != '/' && cu.homepath.back() != '\\')
			cu.homepath += '/';
	}
	else
		cu.homepath = "./";
	std::ofstream metaf(cu.targetmetapath, std::ios::out);
	try {
		ParseTopLevel();
		cu.Phase0();
		cu.Phase1();
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
	cu.Generate();
	SeralizeMetadata(metaf);
    return 0;
}

