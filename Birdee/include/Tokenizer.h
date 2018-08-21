#pragma once
#include <map>
#include <string>
//#include <sstream>      // std::stringbuf
#include "SourcePos.h"
#include "TokenDef.h"
#include <assert.h>
#include <istream>
#include <memory>
//modified based on LLVM tutorial
//https://llvm.org/docs/tutorial/LangImpl02.html



namespace Birdee
{


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

	class Stream
	{
	public:
		virtual ~Stream() {};
		virtual int Getc() = 0;
	};

	class FileStream: public Stream
	{
		FILE* f;
	public:
		bool Okay() { return f; }
		FileStream(const char* path)
		{
			f = fopen(path, "r");
		}
		virtual ~FileStream()
		{
			fclose(f);
		}
		virtual int Getc()
		{
			return ::getc(f);
		}
	};

	class StringStream: public Stream
	{
		std::string str;
		int pos = 0;
	public:
		StringStream(std::string && s):str(std::move(s))
		{}
		virtual ~StringStream()
		{}
		virtual int Getc()
		{
			if (pos >= str.length())
				return EOF;
			return str[pos++];
		}
	};

	class Tokenizer
	{

		std::unique_ptr<Stream> f;
		
		int line;
		int pos;
		int LastChar = ' ';
		std::string template_source;
	public:
		int source_idx;
		bool is_recording = false;
	private:
		int GetChar()
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


		void ParseString()
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
						throw TokenizerError(line, pos, "Unknown escape sequence \\"+(char)LastChar);
					}
					break;
				default:
					IdentifierStr += (char)LastChar;
				}
				LastChar = GetChar();
			}
		}
	public:
		void StartRecording(const std::string& s)
		{
			template_source = s;
			template_source+=LastChar;
			assert(is_recording == false);
			is_recording = true;
		}

		std::string&& StopRecording()
		{
			assert(is_recording);
			is_recording = false;
			template_source.pop_back();
			return std::move(template_source);
		}

		SourcePos GetSourcePos()
		{
			return SourcePos(source_idx,line, pos);
		}

		int GetLine() { return line; }
		int GetPos() { return pos; }
		Token CurTok =tok_add;
		Token GetNextToken() { return CurTok = gettok(); }

		Tokenizer(Tokenizer&& old_t)
		{
			*this = std::move(old_t);
		}

		Tokenizer& operator =(Tokenizer&& old_t)
		{
			f = std::move(old_t.f);
			source_idx = old_t.source_idx;
			line = old_t.line;
			pos = old_t.pos;
			LastChar = old_t.LastChar;
			IdentifierStr = std::move(old_t.IdentifierStr);
			is_recording = old_t.is_recording;
			NumVal = old_t.NumVal;
			template_source = std::move(old_t.template_source);
			return *this;
		}

		Tokenizer(std::unique_ptr<Stream>&& f,int source_idx):f(std::move(f)),source_idx (source_idx){ line = 1; pos = 1;}
		NumberLiteral NumVal;		// Filled in if tok_number

		static std::map<int, Token> single_operator_map;
		static std::map<int, Token> single_token_map;
		static std::map < std::string, Token > token_map;
		std::string IdentifierStr; // Filled in if tok_identifier



		/// gettok - Return the next token from standard input.
		Token gettok() {
			if (!f)
				return tok_error;
			// Skip any whitespace.
			while (isspace(LastChar)&& LastChar!='\n')
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
				while (single_operator_map.find(LastChar=GetChar())!= single_operator_map.end())
					IdentifierStr += LastChar;
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
			if (isalpha(LastChar)|| LastChar=='_') { // identifier: [a-zA-Z][a-zA-Z0-9]*
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
				for(;;)
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
					if(isfloat)
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
