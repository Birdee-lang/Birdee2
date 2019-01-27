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
		SourcePos pos;
		std::string msg;
		CompileError() :pos(-1,1,1){}
		CompileError(SourcePos pos, const std::string& _msg) : pos(pos), msg(_msg) {
			last_error = *this;
		}
		CompileError(const std::string& _msg) : pos(GetCurrentSourcePos()), msg(_msg) {
			last_error = *this;
		}
		void print()
		{
			printf("Compile Error at %s : %s", pos.ToString().c_str(), msg.c_str());
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
			throw CompileError(pos, msg);
		return std::move(p);
	}
}