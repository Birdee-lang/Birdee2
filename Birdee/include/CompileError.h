#pragma once
#include <string>
#include <SourcePos.h>
#include <LibDef.h>

namespace Birdee
{
	BD_CORE_API extern SourcePos GetCurrentSourcePos();
	BD_CORE_API extern std::string GetTemplateStackTrace();
	class CompileError {
	public:
		static BD_CORE_API CompileError last_error;
		int linenumber;
		int pos;
		std::string msg;
		CompileError() :linenumber(0), pos(0) {}
		CompileError(int _linenumber, int _pos, const std::string& _msg) : linenumber(_linenumber), pos(_pos), msg(_msg) {
			last_error = *this;
		}
		CompileError(const std::string& _msg) : linenumber(GetCurrentSourcePos().line), pos(GetCurrentSourcePos().pos), msg(_msg) {
			last_error = *this;
		}
		void print()
		{
			printf("Compile Error at line %d, postion %d : %s", linenumber, pos, msg.c_str());
			auto ret = GetTemplateStackTrace();
			if (!ret.empty())
				printf("\nTemplate stack trace:\n%s", ret.c_str());
		}
	};
	template<typename T>
	T CompileExpectNotNull(T&& p, const std::string& msg)
	{
		SourcePos pos = GetCurrentSourcePos();
		if (!p)
			throw CompileError(pos.line, pos.pos, msg);
		return std::move(p);
	}
}