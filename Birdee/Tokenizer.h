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
		tok_func,

		// primary
		tok_identifier,
		tok_number,
		tok_left_bracket,
		tok_right_bracket,
		tok_newline,
		tok_dim,
		tok_as,
		tok_comma,
		tok_end,

		//type
		tok_int,
		tok_auto,
		tok_void,

		tok_add,
		tok_mul,
		tok_div,
		tok_mod,
		tok_assign
	};


	class TokenizerError {
		int linenumber;
		int pos;
		std::string msg;
	public:
		TokenizerError(int _linenumber, int _pos, const std::string& _msg) : linenumber(_linenumber), pos(_pos), msg(_msg) {}
		void print()
		{
			printf("Compile Error at line %d, postion %d : %s", linenumber, pos, msg.c_str());
		}
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
			pos++;
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
		{"function",tok_func},
		{"end",tok_end},
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
				}
				else
				{
					while (LastChar != EOF && LastChar != '\n' && LastChar != '\r')
						LastChar = GetChar();
				}

				

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
