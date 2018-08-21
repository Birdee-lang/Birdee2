
#include "Tokenizer.h"
#include "AST.h"
#include "CompileError.h"
#include <fstream>
#include <cstdlib>
#include <fstream>
using namespace Birdee;

extern int ParseTopLevel();
extern void SeralizeMetadata(std::ostream& out);

namespace Birdee
{
	std::vector<std::string> source_paths;
	CompileUnit cu;
	std::map<int, Token> Tokenizer::single_operator_map = {
		{ '+',tok_add },
		{'-',tok_minus},
		{ '=',tok_assign },
		{ '*',tok_mul },
		{ '/',tok_div },
		{ '%',tok_mod },
		{ '>',tok_gt },
		{ '<',tok_lt },
		{ '&',tok_and },
		{ '|',tok_or },
		{ '!',tok_not },
		{ '^',tok_xor },
	};
	std::map<int, Token> Tokenizer::single_token_map = {
	{'(',tok_left_bracket },
	{')',tok_right_bracket },
	{ '[',tok_left_index },
	{ ']',tok_right_index },
	{'\n',tok_newline},
	{ ',',tok_comma },
	{'.',tok_dot},
	{':',tok_colon },
	{'@',tok_at},
	};

	std::map < std::string, Token > Tokenizer::token_map = {
		{"new",tok_new},
		{ "dim",tok_dim },
	{"alias",tok_alias},
	{ "true",tok_true },
	{ "false",tok_false },
	{"break",tok_break},
	{ "continue",tok_continue },
	{ "to",tok_to },
	{ "till",tok_till },
	{ "for",tok_for },
	{ "as",tok_as },
	{ "int",tok_int },
	{ "long",tok_long},
	{ "byte",tok_byte },
	{ "ulong",tok_ulong},
	{ "uint",tok_uint},
	{ "float",tok_float},
	{ "double",tok_double},
	{"package",tok_package},
	{"import",tok_import},
	{"boolean",tok_boolean},
	{"function",tok_func},
	{"declare",tok_declare},
	{"addressof",tok_address_of},
	{ "pointerof",tok_pointer_of },
	{"pointer",tok_pointer},
	{"end",tok_end},
	{"class",tok_class},
	{"private",tok_private},
	{"public",tok_public},
	{"if",tok_if},
	{"this",tok_this},
	{"then",tok_then},
	{"else",tok_else},
	{"return",tok_return},
	{"for",tok_for},
	{"null",tok_null},
	{"==",tok_equal},
	{ "!=",tok_ne },
	{"===",tok_cmp_equal},
	{">=",tok_ge},
	{"<=",tok_le},
	{"&&",tok_logic_and},
	{"||",tok_logic_or},
	};
}




Tokenizer tokenizer(nullptr,0);

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
			cu.targetmetapath = target.substr(0, found) + ".bmm";
		}
		else
		{
			cu.targetmetapath = target + ".bmm";
		}
		cu.targetpath = target;
		auto f=std::make_unique<FileStream>(source.c_str());
		if (!f->Okay())
		{
			std::cerr << "Error when opening file " << source << "\n";
			exit(3);
		}
		Birdee::source_paths.push_back(source);
		tokenizer = Tokenizer(std::move(f),0);
		return;
	}
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
	SeralizeMetadata(metaf);
    return 0;
}

