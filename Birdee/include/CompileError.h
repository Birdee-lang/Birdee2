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
		std::string GetMessage() const
		{
			std::string str = "Compile Error at ";
			str += pos.ToString(); str += " : ";str += msg;
			auto ret = GetTemplateStackTrace();
			if (!ret.empty())
			{
				str += "\nTemplate stack trace:\n";
				str += ret;
			}
			return str;
		}
		void print()
		{
			printf("%s", GetMessage().c_str());
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