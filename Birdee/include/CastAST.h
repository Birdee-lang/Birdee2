#pragma once
#include "BdAST.h"
#include <cassert>
#include <Util.h>
namespace Birdee
{
	template <Token typefrom, Token typeto>
	class BD_CORE_API CastNumberExpr : public ExprAST
	{
	public:
		unique_ptr<ExprAST> expr;
		void Phase1() override {};
		unique_ptr<StatementAST> Copy()
		{
			return make_unique<CastNumberExpr>(unique_ptr_cast<ExprAST>(expr->Copy()), Pos);
		}
		void print(int level)
		{
			ExprAST::print(level); std::cout << "Cast " << GetTokenString(typefrom) << " -> " << GetTokenString(typeto) << "\n";
			expr->print(level + 1);
		}
		CastNumberExpr(unique_ptr<ExprAST>&& _expr , SourcePos pos):expr(std::move(_expr))
		{
			Pos = pos;
			this->resolved_type.type = typeto;
			assert(expr->resolved_type.type == typefrom && "expr->resolved_type.type==typefrom");
		}
		llvm::Value* Generate();
	};
	class BD_CORE_API UpcastExprAST : public ExprAST
	{
	public:
		ClassAST* target;
		unique_ptr<ExprAST> expr;
		void Phase1() override {};
		unique_ptr<StatementAST> Copy();
		void print(int level)
		{
			ExprAST::print(level); 
			std::cout << "upcast " << expr->resolved_type.class_ast->GetUniqueName() << " -> " << target->GetUniqueName() << "\n";
			expr->print(level + 1);
		}
		UpcastExprAST(unique_ptr<ExprAST>&& _expr, ClassAST* target, SourcePos pos) :expr(std::move(_expr)),target(target)
		{
			Pos = pos;
			this->resolved_type = ResolvedType(target);
		}
		llvm::Value* Generate();
	};
}