#pragma once
#include <BdAST.h>
#include <Util.h>

namespace Birdee
{
	template <typename T1, typename T2, typename T3, typename T4,
		typename T5, typename T6, typename T7, typename T8>
	void RunOnTemplateArg(ExprAST* ptr, T1 on_basic_type, T2 on_identifier, T3 on_member,
		T4 on_template_ins, T5 on_index, T6 on_number, T7 on_str,T8 on_other)
	{
#define CALL(NAME,FUNC) 	if (instance_of<NAME>(ptr)) \
		{\
			FUNC((NAME*)ptr);\
			return;\
		}\
		
		CALL(BasicTypeExprAST,on_basic_type);
		CALL(IdentifierExprAST, on_identifier);
		CALL(MemberExprAST, on_member);
		CALL(FunctionTemplateInstanceExprAST, on_template_ins);
		CALL(IndexExprAST, on_index);
		CALL(NumberExprAST, on_number);
		CALL(StringLiteralAST, on_str);
		on_other();
#undef CALL
	}


}
