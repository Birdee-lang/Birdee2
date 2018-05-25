#pragma once
#include "AST.h"
#include <cassert>
namespace Birdee
{
	template <Token typefrom, Token typeto>
	class CastNumberExpr : public ExprAST
	{
		unique_ptr<ExprAST> expr;
	public:
		void Phase1() override {};
		CastNumberExpr(unique_ptr<ExprAST>&& _expr):expr(std::move(_expr))
		{
			this->resolved_type.type = typeto;
			assert(expr->resolved_type.type == typefrom && "expr->resolved_type.type==typefrom");
		}
	};
}