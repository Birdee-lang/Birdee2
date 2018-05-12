#pragma once
#include <map>

#include <string>

//modified based on LLVM tutorial
//https://llvm.org/docs/tutorial/LangImpl02.html



namespace Birdee
{

	enum Token {
		tok_error = 0,
		tok_eof = 1,

		// commands
		tok_def,
		tok_extern,

		// primary
		tok_identifier,
		tok_number,
		tok_left_bracket,
		tok_right_bracket,
		tok_newline,
		tok_dim,
		tok_as,
		tok_comma,

		//type
		tok_int,

		tok_add,
		tok_mul,
		tok_div,
		tok_mod,
		tok_assign
	};

	class Tokenizer
	{

		FILE * f;
		int line;
		int pos;

		int GetChar()
		{
			int c = getc(f);
			if (c == '\n')
			{
				line++;
				pos = 0;
			}
			return c;
		}

	public:
		int GetLine() { return line; }
		int GetPos() { return pos; }
		Token CurTok =tok_add;
		Token GetNextToken() { return CurTok = gettok(); }

		Tokenizer(FILE* file) { f = file; line = 1; pos = 1; }



		std::map<int, Token> single_token_map={
		{'(',tok_left_bracket },
		{')',tok_right_bracket },
		{ '+',tok_add },
		{'\n',tok_newline},
		{',',tok_comma},
		{'=',tok_assign},
		{'*',tok_mul},
		{'/',tok_div},
		{ '%',tok_mod },
		};
		std::map < std::string , Token > token_map = {
			{ "dim",tok_dim },
		{ "as",tok_as },
		{ "int",tok_int },
		};
		std::string IdentifierStr; // Filled in if tok_identifier
		double NumVal;             // Filled in if tok_number

		/// gettok - Return the next token from standard input.
		Token gettok() {
			static int LastChar = ' ';

			// Skip any whitespace.
			while (isspace(LastChar)&& LastChar!='\n')
				LastChar = GetChar();

			auto single_token = single_token_map.find(LastChar);
			if (single_token != single_token_map.end())
			{
				LastChar = GetChar();
				return single_token->second;
			}
			if (isalpha(LastChar)|| LastChar=='_') { // identifier: [a-zA-Z][a-zA-Z0-9]*
				IdentifierStr = LastChar;
				while (isalnum((LastChar = GetChar())))
					IdentifierStr += LastChar;

				auto single_token = token_map.find(IdentifierStr);
				if (single_token != token_map.end())
				{
					return single_token->second;
				}
				return tok_identifier;
			}

			if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9.]+
				std::string NumStr;
				do {
					NumStr += LastChar;
					LastChar = GetChar();
				} while (isdigit(LastChar) || LastChar == '.');

				NumVal = strtod(NumStr.c_str(), nullptr);
				return tok_number;
			}



			if (LastChar == '#') {
				// Comment until end of line.
				do
					LastChar = GetChar();
				while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

				if (LastChar != EOF)
					return gettok();
			}

			// Check for end of file.  Don't eat the EOF.
			if (LastChar == EOF)
				return tok_eof;

			// Otherwise, just return the character as its ascii value.
			int ThisChar = LastChar;
			LastChar = GetChar();
			return tok_error;
		}

	};
}
