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
	public:
		static BD_CORE_API TokenizerError last_error;
		int linenumber;
		int pos;
		std::string msg;
		TokenizerError(int _linenumber, int _pos, const std::string& _msg) : linenumber(_linenumber), pos(_pos), msg(_msg) {
			last_error = *this;
		}
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

	class BD_CORE_API Tokenizer
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
		int GetChar();
		void ParseString();
	public:
		void SetLine(int ln) { line = ln; }
		void StartRecording(const std::string& s);
		int GetTemplateSourcePosition()
		{
			return template_source.size();
		}

		std::string&& StopRecording();

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

		Tokenizer& operator =(Tokenizer&& old_t);

		Tokenizer(std::unique_ptr<Stream>&& f,int source_idx):f(std::move(f)),source_idx (source_idx){ line = 1; pos = 1;}
		NumberLiteral NumVal;		// Filled in if tok_number

		static  std::map<int, Token> single_operator_map;
		static  std::map<int, Token> single_token_map;
		static  std::map < std::string, Token > token_map;
		std::string IdentifierStr; // Filled in if tok_identifier



		/// gettok - Return the next token from standard input.
		Token gettok();

	};
}
