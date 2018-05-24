#pragma once
#include <string>
#include <SourcePos.h>
namespace Birdee
{
	extern SourcePos GetCurrentSourcePos();
	class CompileError {
		int linenumber;
		int pos;
		std::string msg;
	public:
		CompileError(int _linenumber, int _pos, const std::string& _msg) : linenumber(_linenumber), pos(_pos), msg(_msg) {}
		CompileError(const std::string& _msg) : linenumber(GetCurrentSourcePos().line), pos(GetCurrentSourcePos().pos), msg(_msg) {}
		void print()
		{
			printf("Compile Error at line %d, postion %d : %s", linenumber, pos, msg.c_str());
		}
	};
	template<typename T>
	T CompileExpectNotNull(T&& p, const std::string& msg)
	{
		if (!p)
			throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), msg);
		return std::move(p);
	}
}