#pragma once

#include <memory>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <functional>
#include "SourcePos.h"
#include "Util.h"
#include "TokenDef.h"

namespace std
{
	bool operator < (reference_wrapper<const string> a, reference_wrapper<const string> b)
	{
		return a.get() < b.get();
	}
	bool operator == (reference_wrapper<const string> a, reference_wrapper<const string> b)
	{
		return a.get() == b.get();
	}
	template <>
	struct hash<reference_wrapper<const string>>
	{
		std::size_t operator()(reference_wrapper<const string> a) const
		{
			hash<string> has;
			return has(a);
		}
	};
}

namespace Birdee {
	using std::string;
	using std::make_unique;
	using std::unique_ptr;
	using std::vector;
	using std::unordered_map;
	using std::reference_wrapper;
	void formatprint(int level) {
		for (int i = 0; i < level; i++)
			std::cout << "\t";
	}
	extern SourcePos GetCurrentSourcePos();
	extern string GetTokenString(Token tok);
	/// StatementAST - Base class for all expression nodes.
	class StatementAST {
	public:
		SourcePos Pos=GetCurrentSourcePos();
		virtual ~StatementAST() = default;
		virtual void print(int level) {
			for (int i = 0; i < level; i++)
				std::cout << "\t";
		}
	};

	class ExprAST : public StatementAST {
	public:
		virtual ~ExprAST() = default;
	};

	class ClassAST;
	class FunctionAST;
	class VariableSingleDefAST;
	class CompileUnit
	{
	public:
		string name;
		vector<unique_ptr<StatementAST>> toplevel;
		unordered_map<std::reference_wrapper<const string>, std::reference_wrapper<ClassAST>> classmap;
		unordered_map<std::reference_wrapper<const string>, std::reference_wrapper<FunctionAST>> funcmap;
		unordered_map<std::reference_wrapper<const string>, std::reference_wrapper<VariableSingleDefAST>> dimmap;

	};
	CompileUnit cu;

	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class NumberExprAST : public ExprAST {
		NumberLiteral Val;
	public:
		NumberExprAST(const NumberLiteral& Val) : Val(Val) {}
		void print(int level) {
			ExprAST::print(level);
			switch (Val.type)
			{
			case const_int:
				std::cout << "const int " << Val.v_int << "\n";
				break;
			case const_long:
				std::cout << "const long " << Val.v_long << "\n";
				break;
			case const_uint:
				std::cout << "const uint " << Val.v_uint << "\n";
				break;
			case const_ulong:
				std::cout << "const ulong " << Val.v_ulong << "\n";
				break;
			case const_float:
				std::cout << "const float " << Val.v_double << "\n";
				break;
			case const_double:
				std::cout << "const double " << Val.v_double << "\n";
				break;
			}
		}
	};

	class ReturnAST : public StatementAST {
		std::unique_ptr<ExprAST> Val;
	public:
		ReturnAST(std::unique_ptr<ExprAST>&& val, SourcePos pos) : Val(std::move(val)) { Pos = pos; };
		void print(int level) {
			StatementAST::print(level); std::cout << "Return\n";
			Val->print(level + 1);
		}
	};

	class StringLiteralAST : public ExprAST {
		std::string Val;
	public:
		StringLiteralAST(const std::string& Val) : Val(Val) {}
		void print(int level) { ExprAST::print(level); std::cout << "\"" << Val << "\"\n"; }
	};

	/// IdentifierExprAST - Expression class for referencing a variable, like "a".
	class IdentifierExprAST : public ExprAST {
		std::string Name;
	public:
		IdentifierExprAST(const std::string &Name) : Name(Name) {}
		void print(int level) { ExprAST::print(level); std::cout << "Identifier:" << Name << "\n"; }
	};
	/// IfBlockAST - Expression class for If block.
	class IfBlockAST : public StatementAST {
		std::unique_ptr<ExprAST> cond;
		std::vector<std::unique_ptr<StatementAST>> iftrue;
		std::vector<std::unique_ptr<StatementAST>> iffalse;
	public:
		IfBlockAST(std::unique_ptr<ExprAST>&& cond,
			std::vector<std::unique_ptr<StatementAST>>&& iftrue,
			std::vector<std::unique_ptr<StatementAST>>&& iffalse,
			SourcePos pos)
			: cond(std::move(cond)), iftrue(std::move(iftrue)), iffalse(std::move(iffalse)) {
			Pos = pos;
		}

		void print(int level) {
			StatementAST::print(level);	std::cout << "If" << "\n";
			cond->print(level + 1);
			StatementAST::print(level + 1);	std::cout << "Then" << "\n";
			for (auto&& s : iftrue)
				s->print(level + 2);
			StatementAST::print(level + 1);	std::cout << "Else" << "\n";
			for (auto&& s : iffalse)
				s->print(level + 2);
		}
	};

	/// BinaryExprAST - Expression class for a binary operator.
	class BinaryExprAST : public ExprAST {
		Token Op;
		std::unique_ptr<ExprAST> LHS, RHS;
	public:
		BinaryExprAST(Token Op, std::unique_ptr<ExprAST>&& LHS,
			std::unique_ptr<ExprAST>&& RHS)
			: Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
		BinaryExprAST(Token Op, std::unique_ptr<ExprAST>&& LHS,
			std::unique_ptr<ExprAST>&& RHS, SourcePos pos)
			: Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {
			Pos = pos;
		}
		void print(int level) {
			ExprAST::print(level);
			std::cout << "OP:" << GetTokenString(Op) << "\n";
			LHS->print(level + 1);
			ExprAST::print(level + 1); std::cout << "----------------\n";
			RHS->print(level + 1);
		}
	};
	/// BinaryExprAST - Expression class for a binary operator.
	class IndexExprAST : public ExprAST {
		std::unique_ptr<ExprAST> Expr, Index;
	public:
		IndexExprAST(std::unique_ptr<ExprAST>&& Expr,
			std::unique_ptr<ExprAST>&& Index, SourcePos Pos)
			: Expr(std::move(Expr)), Index(std::move(Index)) {
			this->Pos = Pos;
		}
		IndexExprAST(std::unique_ptr<ExprAST>&& Expr,
			std::unique_ptr<ExprAST>&& Index)
			: Expr(std::move(Expr)), Index(std::move(Index)) {}
		void print(int level) {
			ExprAST::print(level);
			std::cout << "Index\n";
			Expr->print(level + 1);
			ExprAST::print(level + 1); std::cout << "----------------\n";
			Index->print(level + 1);
		}
	};
	/// CallExprAST - Expression class for function calls.
	class CallExprAST : public ExprAST {
		std::unique_ptr<ExprAST> Callee;
		std::vector<std::unique_ptr<ExprAST>> Args;

	public:

		void print(int level)
		{
			ExprAST::print(level); std::cout << "Call\n";
			Callee->print(level + 1);
			formatprint(level + 1); std::cout << "---------\n";
			for (auto&& n : Args)
				n->print(level + 1);

		}
		CallExprAST(std::unique_ptr<ExprAST> &&Callee,
			std::vector<std::unique_ptr<ExprAST>>&& Args)
			: Callee(std::move(Callee)), Args(std::move(Args)) {}
	};

	/// MemberExprAST - Expression class for function calls.
	class MemberExprAST : public ExprAST {
		std::unique_ptr<ExprAST> Obj;
		std::string member;

	public:

		void print(int level)
		{
			ExprAST::print(level); std::cout << "Member\n";
			Obj->print(level + 1);
			formatprint(level + 1); std::cout << "---------\n";
			formatprint(level + 1); std::cout << member << "\n";

		}
		MemberExprAST(std::unique_ptr<ExprAST> &&Obj,
			const std::string& member)
			: Obj(std::move(Obj)), member(member) {}
	};

	class Type {

	public:
		Token type;
		int index_level = 0;
		virtual ~Type() = default;
		Type(Token _type) :type(_type) {}
		virtual void print(int level)
		{
			formatprint(level);
			std::cout << "Type " << type << " index: " << index_level;
		}
	};

	class IdentifierType :public Type {
		std::string name;
	public:
		IdentifierType(const std::string&_name) :Type(tok_identifier), name(_name) {}
		void print(int level)
		{
			Type::print(level);
			std::cout << " Name: " << name;
		}
	};

	class VariableSingleDefAST;

	class VariableDefAST : public StatementAST {
	public:
		virtual void move(unique_ptr<VariableDefAST>&& current,
			std::function<void(unique_ptr<VariableSingleDefAST>&&)> func) = 0;
	};



	class VariableSingleDefAST : public VariableDefAST {

		std::unique_ptr<Type> type;
		std::unique_ptr<ExprAST> val;
	public:
		void move(unique_ptr<VariableDefAST>&& current,
			std::function<void(unique_ptr<VariableSingleDefAST>&&)> func)
		{
			func(dynamic_unique_ptr_cast<VariableSingleDefAST, VariableDefAST>(std::move(current)));
		}
		std::string name;
		VariableSingleDefAST(const std::string& _name, std::unique_ptr<Type>&& _type, std::unique_ptr<ExprAST>&& _val) : name(_name), type(std::move(_type)), val(std::move(_val)) {}
		VariableSingleDefAST(const std::string& _name, std::unique_ptr<Type>&& _type, std::unique_ptr<ExprAST>&& _val, SourcePos Pos) : name(_name), type(std::move(_type)), val(std::move(_val)) {
			this->Pos = Pos;
		}
		void print(int level) {
			VariableDefAST::print(level);
			std::cout << "Variable:" << name << " ";
			type->print(0);
			std::cout << "\n";
			if (val)
				val->print(level + 1);
		}
	};

	class VariableMultiDefAST : public VariableDefAST {

	public:
		std::vector<std::unique_ptr<VariableSingleDefAST>> lst;
		VariableMultiDefAST(std::vector<std::unique_ptr<VariableSingleDefAST>>&& vec) :lst(std::move(vec)) {}
		VariableMultiDefAST(std::vector<std::unique_ptr<VariableSingleDefAST>>&& vec, SourcePos pos) :lst(std::move(vec)) {
			Pos = pos;
		}
		void move(unique_ptr<VariableDefAST>&& current,
			std::function<void(unique_ptr<VariableSingleDefAST>&&)> func)
		{
			for (auto&& var : lst)
				func(std::move(var));
		}

		void print(int level) {
			//VariableDefAST::print(level);
			for (auto& a : lst)
				a->print(level);
		}
	};


	/// PrototypeAST - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes).
	class PrototypeAST {
	protected:
		std::string Name;
		std::unique_ptr<VariableDefAST> Args;
		std::unique_ptr<Type> RetType;

	public:
		PrototypeAST(const std::string &Name, std::unique_ptr<VariableDefAST>&& Args, std::unique_ptr<Type>&& RetType)
			: Name(Name), Args(std::move(Args)), RetType(std::move(RetType)) {}

		const std::string &GetName() const { return Name; }
		void print(int level)
		{
			formatprint(level);
			std::cout << "Function Proto: " << Name << std::endl;
			if (Args)
				Args->print(level + 1);
			else
			{
				formatprint(level + 1); std::cout << "No arg\n";
			}
			RetType->print(level + 1); std::cout << "\n";
		}
	};

	/// FunctionAST - This class represents a function definition itself.
	class FunctionAST : public ExprAST {
		std::unique_ptr<PrototypeAST> Proto;
		std::vector<std::unique_ptr<StatementAST>> Body;

	public:
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
			std::vector<std::unique_ptr<StatementAST>>&& Body)
			: Proto(std::move(Proto)), Body(std::move(Body)) {}
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
			std::vector<std::unique_ptr<StatementAST>>&& Body, SourcePos pos)
			: Proto(std::move(Proto)), Body(std::move(Body)) {
			Pos = pos;
		}

		const string& GetName()
		{
			return Proto->GetName();
		}
		void print(int level)
		{
			ExprAST::print(level);
			std::cout << "Function def\n";
			Proto->print(level + 1);
			for (auto&& node : Body)
			{
				node->print(level + 1);
			}
		}
	};

	enum AccessModifier
	{
		access_public,
		access_private
	};
	class FieldDef
	{
	public:
		AccessModifier access;
		std::unique_ptr<VariableSingleDefAST> decl;
		void print(int level)
		{
			formatprint(level);
			std::cout << "Member ";
			if (access == access_private)
				std::cout << "private variable";
			else
				std::cout << "public variable";
			decl->print(level);
		}
		FieldDef(AccessModifier access, std::unique_ptr<VariableSingleDefAST>&& decl) :access(access), decl(std::move(decl)) {}
	};
	class MemberFunctionDef
	{
	public:
		AccessModifier access;
		std::unique_ptr<FunctionAST> decl;
		void print(int level)
		{
			formatprint(level);
			std::cout << "Member ";
			if (access == access_private)
				std::cout << "private variable";
			else
				std::cout << "public variable";
			decl->print(level);
		}
		MemberFunctionDef(AccessModifier access, std::unique_ptr<FunctionAST>&& decl) :access(access), decl(std::move(decl)) {}
	};
	class ClassAST : public StatementAST {

		std::vector<FieldDef> fields;
		std::vector<MemberFunctionDef> funcs;
		unordered_map<reference_wrapper<const string>, reference_wrapper<FieldDef>> fieldmap;
		unordered_map<reference_wrapper<const string>, reference_wrapper<MemberFunctionDef>> funcmap;
	public:
		std::string name;

		ClassAST(const std::string& name,
			std::vector<FieldDef>&& fields, std::vector<MemberFunctionDef>&& funcs,
			unordered_map<reference_wrapper<const string>, reference_wrapper<FieldDef>> fieldmap,
			unordered_map<reference_wrapper<const string>, reference_wrapper<MemberFunctionDef>> funcmap,
			SourcePos pos)
			: name(name), fields(std::move(fields)), funcs(std::move(funcs)),
			fieldmap(std::move(fieldmap)), funcmap(std::move(funcmap)) {
			Pos = pos;
		}
		void print(int level)
		{
			StatementAST::print(level);
			std::cout << "Class def: " << name << std::endl;
			for (auto& node : fields)
			{
				node.print(level + 1);
			}
			for (auto& node : funcs)
			{
				node.print(level + 1);
			}
		}
	};



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
}