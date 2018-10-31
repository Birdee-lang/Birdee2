#include <BdAST.h>
#include <CompileError.h>
#include <Tokenizer.h>

using namespace Birdee;
CompileError CompileError::last_error;
TokenizerError TokenizerError::last_error(0, 0, "");

namespace Birdee
{
	BD_CORE_API std::vector<std::string> source_paths;
	CompileUnit cu;
	BD_CORE_API std::map<int, Token> Tokenizer::single_operator_map = {
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
	BD_CORE_API std::map<int, Token> Tokenizer::single_token_map = {
	{'(',tok_left_bracket },
	{')',tok_right_bracket },
	{ '[',tok_left_index },
	{ ']',tok_right_index },
	{'\n',tok_newline},
	{ ',',tok_comma },
	{'.',tok_dot},
	{':',tok_colon },
	};

	BD_CORE_API std::map < std::string, Token > Tokenizer::token_map = {
		{"new",tok_new},
		{"dim",tok_dim },
		{"while",tok_while},
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
	{"!==",tok_cmp_ne},
	{">=",tok_ge},
	{"<=",tok_le},
	{"&&",tok_logic_and},
	{"||",tok_logic_or},
	};
}

extern void ClearPreprocessingState();
extern void RunGenerativeScript();
void Birdee::CompileUnit::Clear()
{
	toplevel.clear();
	classmap.clear();
	funcmap.clear();
	dimmap.clear();
	symbol_prefix.clear();

	imported_classmap.clear();
	imported_funcmap.clear();
	imported_dimmap.clear();

	imported_class_templates.clear();
	imported_func_templates.clear();
	orphan_class.clear();

	imported_module_names.clear();
	imported_packages.map.clear();
	imported_packages.mod = nullptr;
	ClearPreprocessingState();
	AbortGenerate();
}


BD_CORE_API Tokenizer tokenizer(nullptr, 0);

static struct Initializer
{
	Initializer()
	{
		char* home = std::getenv("BIRDEE_HOME");
		if (home)
		{
			cu.homepath = home;
			if (cu.homepath.back() != '/' && cu.homepath.back() != '\\')
				cu.homepath += '/';
		}
		else
			cu.homepath = "./";

	}

}mod_initializer;