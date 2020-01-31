#include <BdAST.h>
#include <CompileError.h>
#include <Tokenizer.h>
#include <CompilerOptions.h>

using namespace Birdee;
CompileError CompileError::last_error;
TokenizerError TokenizerError::last_error(0, 0, "");

namespace Birdee
{
	BD_CORE_API std::vector<std::string> source_paths;
	CompileUnit cu;

	unique_ptr<CompilerOptions> DefaultCompilerOptions()
	{
		return make_unique<CompilerOptions>();
	}
	CompileUnit::~CompileUnit() {}
	CompileUnit::CompileUnit() {
		options = DefaultCompilerOptions();
	}

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
		{'.',tok_dot},
	};
	BD_CORE_API std::map<int, Token> Tokenizer::single_token_map = {
	{'(',tok_left_bracket },
	{')',tok_right_bracket },
	{ '[',tok_left_index },
	{ ']',tok_right_index },
	{'\n',tok_newline},
	{ ',',tok_comma },
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
	{ "short", tok_short},
	{ "float",tok_float},
	{ "double",tok_double},
	{"package",tok_package},
	{"import",tok_import},
	{"boolean",tok_boolean},
	{"function",tok_func},
	{"func",tok_func},
	{"declare",tok_declare},
	{"abstract",tok_abstract},
	{"addressof",tok_address_of},
	{ "pointerof",tok_pointer_of },
	{"pointer",tok_pointer},
	{"end",tok_end},
	{"class",tok_class},
	{"struct",tok_struct},
	{"interface",tok_interface},
	{"implements",tok_implement},
	{"typeof",tok_typeof},
	{"private",tok_private},
	{"public",tok_public},
	{"if",tok_if},
	{"this",tok_this},
	{"super",tok_super},
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
	{"=>",tok_into},
	{"functype",tok_functype},
	{"closure",tok_closure},
	{"...",tok_ellipsis},
	{"try",tok_try},
	{"catch",tok_catch},
	{"throw",tok_throw},
	{"defer",tok_defer},
	};

	BD_CORE_API bool IsResolvedTypeClass(const ResolvedType& r)
	{
		return r.type == tok_class && r.index_level == 0 && !r.class_ast->is_struct;
	}
}

extern void ClearPreprocessingState();
extern void RunGenerativeScript();
extern void ClearParserState();
void Birdee::CompileUnit::Clear()
{
	toplevel.clear();
	classmap.clear();
	funcmap.clear();
	dimmap.clear();
	functypemap.clear();
	symbol_prefix.clear();

	imported_classmap.clear();
	imported_funcmap.clear();
	imported_dimmap.clear();
	imported_functypemap.clear();
	generated_functy.clear();

	imported_class_templates.clear();
	imported_func_templates.clear();
	orphan_class.clear();

	class_extend_funcmap.clear();
	init_scripts.clear();
	imported_module_names.clear();
	imports.clear();
	imported_packages.~ImportTree();
	new (&imported_packages) ImportTree;

	ClearPreprocessingState();
	AbortGenerate();
	ClearParserState();
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


size_t Birdee::str_view::hash() const
{
	size_t result = 0;
	const size_t prime = 31;
	auto itr = str->begin() + starts;
	for (size_t i = 0; i < len; ++i, ++itr) {
		result = *itr + (result * prime);
	}
	return result;
}

bool Birdee::str_view::operator==(const str_view & other) const
{
	if (other.len != len)
		return false;
	if (other.str == str && starts == other.starts)
		return true;
	return std::equal(str->begin() + starts, str->begin() + (starts + len), other.str->begin() + other.starts);
}

std::string Birdee::str_view::to_string() const
{
	std::string ret;
	ret.reserve(len);
	auto itr = str->begin() + starts;
	for (size_t i = 0; i < len; ++i, ++itr) {
		ret.push_back(*itr);
	}
	return ret;
}

void Birdee::Tokenizer::ParseString()
{
	IdentifierStr = "";
	LastChar = GetChar();
	while (true)
	{
		switch (LastChar)
		{
		case '\n':
			throw TokenizerError(line, pos, "New line is not allowed in simple string literal");
			break;
		case  EOF:
			throw TokenizerError(line, pos, "Unexpected end of file when parsing a string literal: expected \"");
			break;
		case '\"':
			LastChar = GetChar();
			return;
			break;
		case '\\':
			LastChar = GetChar();
			switch (LastChar)
			{
			case 'n':
				IdentifierStr += '\n';
				break;
			case '\\':
				IdentifierStr += '\\';
				break;
			case '\'':
				IdentifierStr += '\'';
				break;
			case '\"':
				IdentifierStr += '\"';
				break;
			case '?':
				IdentifierStr += '\?';
				break;
			case 'a':
				IdentifierStr += '\a';
				break;
			case 'b':
				IdentifierStr += '\b';
				break;
			case 'f':
				IdentifierStr += '\f';
				break;
			case 'r':
				IdentifierStr += '\r';
				break;
			case 't':
				IdentifierStr += '\t';
				break;
			case 'v':
				IdentifierStr += '\v';
				break;
			case '0':
				IdentifierStr += '\0';
				break;
			case EOF:
				throw TokenizerError(line, pos, "Unexpected end of file");
				break;
			default:
				throw TokenizerError(line, pos, "Unknown escape sequence \\" + (char)LastChar);
			}
			break;
		default:
			IdentifierStr += (char)LastChar;
		}
		LastChar = GetChar();
	}
}

std::string&& Birdee::Tokenizer::StopRecording()
{
	assert(is_recording);
	is_recording = false;
	template_source.pop_back();
	return std::move(template_source);
}

void Birdee::Tokenizer::StartRecording(const std::string& s)
{
	template_source = s;
	template_source += LastChar;
	assert(is_recording == false);
	is_recording = true;
}

int Birdee::Tokenizer::GetChar()
{
	int c = f->Getc();
	if (is_recording)
		template_source += c;
	if (c == '\n')
	{
		line++;
		pos = 0;
	}
	pos++;
	return c;
}

string Birdee::SourcePos::ToString() const
{
	std::stringstream buf;
	buf << "File: " << (source_idx == -1 ? cu.directory+"/"+cu.filename : source_paths[source_idx]) << " Line: " << line << " Pos: " << pos;
	return buf.str();
}

Birdee::Tokenizer& Birdee::Tokenizer::operator =(Tokenizer&& old_t)
{
	f = std::move(old_t.f);
	source_idx = old_t.source_idx;
	line = old_t.line;
	pos = old_t.pos;
	CurTok = old_t.CurTok;
	LastChar = old_t.LastChar;
	IdentifierStr = std::move(old_t.IdentifierStr);
	is_recording = old_t.is_recording;
	NumVal = old_t.NumVal;
	template_source = std::move(old_t.template_source);
	return *this;
}

Token Birdee::Tokenizer::gettok() {
	static_assert(TOKENIZER_PENDING_CHAR != EOF, "expecting TOKENIZER_PENDING_CHAR != EOF");
	if (!f)
		return tok_error;
	if (LastChar == TOKENIZER_PENDING_CHAR)
		LastChar = GetChar();
	// Skip any whitespace.
	while (isspace(LastChar) && LastChar != '\n')
		LastChar = GetChar();
	auto single_token = single_token_map.find(LastChar);
	if (single_token != single_token_map.end())
	{
		LastChar = GetChar();
		return single_token->second;
	}
	single_token = single_operator_map.find(LastChar);
	if (single_token != single_operator_map.end())
	{
		IdentifierStr = LastChar;
		// search in token map for multi-character token
		for (;;)
		{
			LastChar = GetChar();
			// a small performance enhancement: if the character is not in single_operator_map
			// we can just jump out of the loop
			if (single_operator_map.find(LastChar) == single_operator_map.end())
				break;
			IdentifierStr += LastChar;
			// search in token map,find a key in the map that starts with IdentifierStr
			// https://stackoverflow.com/a/9350066
			auto itr = token_map.lower_bound(IdentifierStr);
			if (itr != token_map.end()) {
				const string& key = itr->first;
				if (key.compare(0, IdentifierStr.size(), IdentifierStr) == 0) // Really a prefix?
					continue;
			}
			// a failed match will increase IdentifierStr.size() by 1, pop up the last character
			IdentifierStr.pop_back();
			break;
		}
		if (IdentifierStr.size() > 1)
		{
			auto moperator = token_map.find(IdentifierStr);
			if (moperator != token_map.end())
				return moperator->second;
			else
				throw TokenizerError(line, pos, "Bad token");
		}
		else
		{
			return single_token->second;
		}
	}
	if (isalpha(LastChar) || LastChar == '_') { // identifier: [a-zA-Z][a-zA-Z0-9]*
		IdentifierStr = LastChar;
		LastChar = GetChar();
		while (isalnum(LastChar) || LastChar == '_')
		{
			IdentifierStr += LastChar;
			LastChar = GetChar();
		}

		auto single_token = token_map.find(IdentifierStr);
		if (single_token != token_map.end())
		{
			return single_token->second;
		}
		return tok_identifier;
	}

	if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9.]+
		std::string NumStr;
		bool isfloat = (LastChar == '.');
		for (;;)
		{
			NumStr += LastChar;
			LastChar = GetChar();
			if (!isdigit(LastChar))
			{
				if (LastChar == '.')
				{
					if (isfloat)
						throw TokenizerError(line, pos, "Bad number literal");
					isfloat = true; //we only allow one dot in number literal
					continue;
				}
				break;
			}
		}
		switch (LastChar)
		{
		case 'L':
			if (isfloat)
				throw TokenizerError(line, pos, "Should not have \'L\' after a float literal");
			NumVal.type = tok_long;
			NumVal.v_long = std::stoll(NumStr);
			LastChar = GetChar();
			break;
		case 'U':
			if (isfloat)
				throw TokenizerError(line, pos, "Should not have \'U\' after a float literal");
			NumVal.type = tok_ulong;
			NumVal.v_ulong = std::stoull(NumStr);
			LastChar = GetChar();
			break;
		case 'u':
			if (isfloat)
				throw TokenizerError(line, pos, "Should not have \'u\' after a float literal");
			NumVal.type = tok_uint;
			NumVal.v_uint = std::stoul(NumStr);
			LastChar = GetChar();
			break;
		case 'f':
			NumVal.type = tok_float;
			NumVal.v_double = std::stof(NumStr);
			LastChar = GetChar();
			break;
		case 'd':
			NumVal.type = tok_double;
			NumVal.v_double = std::stod(NumStr);
			LastChar = GetChar();
			break;
		default:
			if (isfloat)
			{
				NumVal.type = tok_float;
				NumVal.v_double = std::stod(NumStr);
			}
			else
			{
				NumVal.type = tok_int;
				NumVal.v_int = std::stoi(NumStr);
			}
		}
		return tok_number;
	}

	if (LastChar == '\"') {
		ParseString();
		return tok_string_literal;
	}
	if (LastChar == '{') {
		IdentifierStr = "";
		LastChar = GetChar();
		if (LastChar != '@')
			throw TokenizerError(line, pos, "Expect \'@\' after \'{\'");
		LastChar = GetChar();
		for (;;)
		{
			if (LastChar == EOF)
				throw TokenizerError(line, pos, "Unexpected end of file: expected @}");
			else if (LastChar == '}' && IdentifierStr.back() == '@')
			{
				IdentifierStr.pop_back();
				LastChar = GetChar();
				break;
			}
			else
			{
				IdentifierStr += (char)LastChar;
				LastChar = GetChar();
			}
		}
		return tok_script;
	}

	if (LastChar == '#') {
		// Comment until end of line.
		LastChar = GetChar();
		if (LastChar == '#')
		{
			int cnt_sharp = 0;
			while (LastChar != EOF)
			{
				LastChar = GetChar();
				if (LastChar == '#')
				{
					cnt_sharp++;
					if (cnt_sharp == 2)
						break;
				}
				else
					cnt_sharp = 0;
			}
			if (LastChar == EOF)
				throw TokenizerError(line, pos, "Unexpected end of file: expected ##");
			LastChar = GetChar();
		}
		else
		{
			while (LastChar != EOF && LastChar != '\n' && LastChar != '\r')
				LastChar = GetChar();
		}



		if (LastChar != EOF)
			return gettok();
	}
	if (LastChar == '\'')
	{
		LastChar = GetChar();
		if (LastChar != '\'')
			throw TokenizerError(line, pos, "Expecting \' for raw string");
		LastChar = GetChar();
		if (LastChar != '\'')
			throw TokenizerError(line, pos, "Expecting \' for raw string");
		int cnt_dot = 0;
		IdentifierStr = "";
		while (LastChar != EOF)
		{
			LastChar = GetChar();
			IdentifierStr += LastChar;
			if (LastChar == '\'')
			{
				cnt_dot++;
				if (cnt_dot == 3)
				{
					IdentifierStr.pop_back(); IdentifierStr.pop_back(); IdentifierStr.pop_back();
					break;
				}
			}
			else
				cnt_dot = 0;
		}
		if (LastChar == EOF)
			throw TokenizerError(line, pos, "Unexpected end of file: expecting \'\'\'");
		LastChar = GetChar();
		return tok_string_literal;
	}
	if (LastChar == '@')
	{
		LastChar = GetChar();
		Token ret = gettok();
		if (ret == tok_private)
			tokenizer.IdentifierStr = "private";
		else if (ret != tok_identifier)
			throw TokenizerError(line, pos, "Expected an identifier after \'@\'");
		if (!isspace(LastChar) && LastChar != '\n') //if annotation is not followed by a " ", parse until the end of the line
		{
			do {
				IdentifierStr += LastChar;
				LastChar = GetChar();
			} while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
		}
		return tok_annotation;
	}
	// Check for end of file.  Don't eat the EOF.
	if (LastChar == EOF)
		return tok_eof;

	// Otherwise, just return the character as its ascii value.
	int ThisChar = LastChar;
	LastChar = GetChar();
	return tok_error;
}