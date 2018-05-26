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
		void print(int level)
		{
			ExprAST::print(level); std::cout << "Cast " << GetTokenString(typefrom) << " -> " << GetTokenString(typeto) << "\n";
			expr->print(level + 1);
		}
		CastNumberExpr(unique_ptr<ExprAST>&& _expr):expr(std::move(_expr))
		{
			this->resolved_type.type = typeto;
			assert(expr->resolved_type.type == typefrom && "expr->resolved_type.type==typefrom");
		}
	};
}