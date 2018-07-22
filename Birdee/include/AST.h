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
	inline bool operator < (reference_wrapper<const string> a, reference_wrapper<const string> b)
	{
		return a.get() < b.get();
	}
	inline bool operator == (reference_wrapper<const string> a, reference_wrapper<const string> b)
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
	template <>
	struct hash<Birdee::Token>
	{
		std::size_t operator()(Birdee::Token t) const
		{
			hash<int> has;
			return has((int)t);
		}
	};
}
namespace llvm
{
	class Value;
	class Function;
	class FunctionType;
	class StructType;
	class DIType;
}
namespace Birdee {
	using std::string;
	using std::unique_ptr;
	using std::vector;
	using std::unordered_map;
	using std::reference_wrapper;
	using ::llvm::Value;
	using std::make_unique;
	/*template<typename T, typename... Args>
	std::unique_ptr<T> make_unique(Args&&... args) {
		return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
	}*/

	

	class ClassAST;
	class FunctionAST;
	class VariableSingleDefAST;

	inline void formatprint(int level) {
		for (int i = 0; i < level; i++)
			std::cout << "\t";
	}

	extern SourcePos GetCurrentSourcePos();
	extern string GetTokenString(Token tok);

	struct ImportedModule
	{
		unordered_map<string, unique_ptr<ClassAST>> classmap;
		unordered_map<string, unique_ptr<FunctionAST>> funcmap;
		unordered_map<string, unique_ptr<VariableSingleDefAST>> dimmap;
		void Init(const vector<string>& package,const string& module_name);
	};

	//a quick structure to find & store names of imported packages
	class ImportTree
	{
	public:
		unordered_map<string, unique_ptr<ImportTree>> map;
		unique_ptr<ImportedModule> mod;
		//if the current node has the name, will not search the next levels
		ImportTree* FindName(const string& name) const;
		//search the path along the path
		ImportTree* Contains(const vector<string>& package,int level=0);
		ImportTree* Contains(const string& package, int sz,int level = 0);
		//create the tree nodes along the path
		ImportTree* Insert(const vector<string>& package, int level=0);
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
		
	public:
		std::string name;
		IdentifierType(const std::string&_name) :Type(tok_identifier), name(_name) {}
		void print(int level)
		{
			Type::print(level);
			std::cout << " Name: " << name;
		}
	};

	class QualifiedIdentifierType :public Type {

	public:
		vector<string> name;
		QualifiedIdentifierType(vector<string>&&_name) :Type(tok_package), name(std::move(_name)) {}
		void print(int level)
		{
			Type::print(level);
			std::cout << " QualifiedName: " << name.back();
		}
	};

	class ClassAST;
	class PrototypeAST;
	extern const string& GetClassASTName(ClassAST*);
	extern bool operator ==(const PrototypeAST& ,const PrototypeAST& );
	class ResolvedType {
		void ResolveType(Type& _type, SourcePos pos);
	public:
		Token type;
		int index_level;
		union {
			ClassAST* class_ast;
			PrototypeAST * proto_ast;
			ImportTree* import_node;
		};
		ResolvedType(Type& _type,SourcePos pos) :type(_type.type),index_level(_type.index_level), class_ast(nullptr)
		{
			ResolveType(_type,pos); 
		}
		ResolvedType():type(tok_error),index_level(0), class_ast(nullptr)
		{

		}
		ResolvedType(ClassAST* cls) :type(tok_class), index_level(0), class_ast(cls)
		{	}
		bool isReference()
		{
			return 	index_level >0 || type == tok_class || type==tok_null;
		}

		bool isNull()
		{
			return 	index_level == 0 && type == tok_null;
		}
		bool isInteger()
		{
			return 	index_level == 0 && type == tok_int
				|| type == tok_long
				|| type == tok_ulong
				|| type == tok_uint
				|| type == tok_byte;
		}
		bool isSigned()
		{
			return 	index_level == 0 && (type == tok_int
				|| type == tok_long || type == tok_byte);
		}
		bool isNumber() const
		{
			return 	index_level == 0 && type == tok_int
				|| type == tok_long
				|| type == tok_ulong
				|| type == tok_uint
				|| type == tok_float
				|| type == tok_double
				|| type == tok_byte;
		}
		bool isResolved() const
		{
			return type!=tok_error;
		}
		string GetString();
		bool operator == (const ResolvedType& other) const
		{
			if (type == other.type && other.index_level == index_level)
			{
				if (type == tok_class)
					return other.class_ast == class_ast;
				else if (type == tok_func)
					return *proto_ast == *other.proto_ast;
				else
					return true;
			}
			return false;
		}
	};

	/// StatementAST - Base class for all expression nodes.
	class StatementAST {
	public:
		virtual Value* Generate()=0;
		SourcePos Pos=GetCurrentSourcePos();
		virtual void Phase1() = 0;
		virtual ~StatementAST() = default;
		virtual void print(int level) {
			for (int i = 0; i < level; i++)
				std::cout << "\t";
		}
	};

	class ExprAST : public StatementAST {
	public:
		ResolvedType resolved_type;
		virtual llvm::Value* GetLValue(bool checkHas) { return nullptr; };
		virtual ~ExprAST() = default;
		void print(int level) {
			StatementAST::print(level); std::cout << "Type: "<< resolved_type.GetString()<<" ";
		}
	};


	class CompileUnit
	{
	public:
		string targetpath;
		string filename;
		string directory;
		string name;
		string targetmetapath;
		string symbol_prefix;
		string homepath;
		bool is_corelib=false;
		bool expose_main = false;
		vector<unique_ptr<StatementAST>> toplevel;
		unordered_map<std::reference_wrapper<const string>, std::reference_wrapper<ClassAST>> classmap;
		unordered_map<std::reference_wrapper<const string>, std::reference_wrapper<FunctionAST>> funcmap;
		unordered_map<std::reference_wrapper<const string>, std::reference_wrapper<VariableSingleDefAST>> dimmap;

		//maps from short names ("import a.b:c" => "c") to the imported AST
		unordered_map<std::reference_wrapper<const string>, ClassAST*> imported_classmap;
		unordered_map<std::reference_wrapper<const string>, FunctionAST*> imported_funcmap;
		unordered_map<std::reference_wrapper<const string>, VariableSingleDefAST*> imported_dimmap;

		/*
		The classes that are referenced by an imported class, but the packages of the 
		referenced classes are not yet imported. The mapping from qualified names of classes to AST
		*/
		unordered_map<string, unique_ptr<ClassAST>> orphan_class;

		vector<string> imported_module_names;
		/*
			for quick module name-looking. We need this because we need to quickly distinguish in "AAA.BBB",
			whether AAA is a variable name or a package name.
		*/
		ImportTree imported_packages;
		void Phase0();
		void Phase1();
		void InitForGenerate();
		void Generate();
	};
	extern  CompileUnit cu;

	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class NumberExprAST : public ExprAST {
		NumberLiteral Val;
	public:
		Value* Generate();
		virtual void Phase1();
		NumberExprAST(const NumberLiteral& Val) : Val(Val) {}
		void print(int level) {
			ExprAST::print(level);
			switch (Val.type)
			{
			case tok_byte:
				std::cout << "const byte " << Val.v_int << "\n";
				break;
			case tok_int:
				std::cout << "const int " << Val.v_int << "\n";
				break;
			case tok_long:
				std::cout << "const long " << Val.v_long << "\n";
				break;
			case tok_uint:
				std::cout << "const uint " << Val.v_uint << "\n";
				break;
			case tok_ulong:
				std::cout << "const ulong " << Val.v_ulong << "\n";
				break;
			case tok_float:
				std::cout << "const float " << Val.v_double << "\n";
				break;
			case tok_double:
				std::cout << "const double " << Val.v_double << "\n";
				break;
			}
		}
	};

	class PrototypeAST;
	class ReturnAST : public StatementAST {
		std::unique_ptr<ExprAST> Val;
		PrototypeAST* proto;
	public:
		Value* Generate();
		virtual void Phase1();
		ReturnAST(std::unique_ptr<ExprAST>&& val, PrototypeAST* proto, SourcePos pos) : Val(std::move(val)), proto(proto){ Pos = pos; };
		void print(int level) {
			StatementAST::print(level); std::cout << "Return\n";
			Val->print(level + 1);
		}
	};

	class StringLiteralAST : public ExprAST {
		std::string Val;
	public:
		virtual void Phase1();
		llvm::Value* Generate();
		StringLiteralAST(const std::string& Val) : Val(Val) {}
		void print(int level) { ExprAST::print(level); std::cout << "\"" << Val << "\"\n"; }
	};

	class ResolvedIdentifierExprAST : public ExprAST {
	public:
		virtual bool isMutable()=0;
	};
	/// IdentifierExprAST - Expression class for referencing a variable, like "a".
	class IdentifierExprAST : public ExprAST {
		std::string Name;
	public:
		unique_ptr<ResolvedIdentifierExprAST> impl;
		void Phase1();
		llvm::Value * GetLValue(bool checkHas) override { return impl->GetLValue(checkHas); };
		llvm::Value* Generate();
		IdentifierExprAST(const std::string &Name) : Name(Name) {}
		void print(int level) { ExprAST::print(level); std::cout << "Identifier:" << Name << "\n"; }
	};

	class ResolvedFuncExprAST : public ResolvedIdentifierExprAST {
		FunctionAST* def;
	public:
		bool isMutable() { return false; }
		ResolvedFuncExprAST(FunctionAST* def, SourcePos pos) :def(def) { Phase1(); Pos = pos; }
		void Phase1();
		llvm::Value* Generate();
	};

	class ThisExprAST : public ExprAST {
	public:
		void Phase1();
		llvm::Value* Generate();
		ThisExprAST()   {}
		ThisExprAST(ClassAST* cls, SourcePos pos) { resolved_type.type = tok_class; resolved_type.class_ast = cls; Pos = pos; }
		void print(int level) { ExprAST::print(level); std::cout << "this" << "\n"; }
	};
	class BoolLiteralExprAST : public ExprAST {
	public:
		bool v;
		void Phase1() {};
		llvm::Value* Generate();
		BoolLiteralExprAST(bool v) : v(v) { resolved_type.type = tok_boolean;}
		void print(int level) { ExprAST::print(level); std::cout << "bool" << "\n"; }
	};

	class NullExprAST : public ExprAST {
	public:
		void Phase1() {};
		llvm::Value* Generate();
		NullExprAST() { resolved_type.type = tok_null; }
		void print(int level) { ExprAST::print(level); std::cout << "null" << "\n"; }
	};
	class ASTBasicBlock
	{
	public:
		std::vector<std::unique_ptr<StatementAST>> body;
		void Phase1();
		void Phase1(PrototypeAST* proto);
		bool Generate();
		void print(int level)
		{
			for (auto&& s : body)
				s->print(level);
		}
	};

	/// IfBlockAST - Expression class for If block.
	class IfBlockAST : public StatementAST {
		std::unique_ptr<ExprAST> cond;
		ASTBasicBlock iftrue;
		ASTBasicBlock iffalse;
	public:
		void Phase1();
		llvm::Value* Generate();
		IfBlockAST(std::unique_ptr<ExprAST>&& cond,
			ASTBasicBlock&& iftrue,
			ASTBasicBlock&& iffalse,
			SourcePos pos)
			: cond(std::move(cond)), iftrue(std::move(iftrue)), iffalse(std::move(iffalse)) {
			Pos = pos;
		}

		void print(int level) {
			StatementAST::print(level);	std::cout << "If" << "\n";
			cond->print(level + 1);
			StatementAST::print(level + 1);	std::cout << "Then" << "\n";
			iftrue.print(level + 2);
			StatementAST::print(level + 1);	std::cout << "Else" << "\n";
			iffalse.print(level + 2);
		}
	};
	/// IfBlockAST - Expression class for If block.
	class ForBlockAST : public StatementAST {
		std::unique_ptr<StatementAST> init;
		ExprAST* loop_var;
		std::unique_ptr<ExprAST> till;
		bool including;
		bool isdim;
		ASTBasicBlock block;
	public:
		void Phase1();
		llvm::Value* Generate();
		ForBlockAST(std::unique_ptr<StatementAST>&& init, std::unique_ptr<ExprAST>&& till,
			bool including,
			bool isdim,
			ASTBasicBlock&& block,
			SourcePos pos)
			: init(std::move(init)), till(std::move(till)), block(std::move(block)),including(including),isdim(isdim) {
			Pos = pos;
		}

		void print(int level) {
			StatementAST::print(level);	std::cout << "For" << "\n";
			init->print(level + 1);
			StatementAST::print(level + 1);	std::cout << "To" << "\n";
			till->print(level + 1);
			StatementAST::print(level + 1);	std::cout << "Block" << "\n";
			block.print(level + 2);
		}
	};
	class LoopControlAST :public StatementAST {
	public:
		Token tok;
		void Phase1() {};
		llvm::Value* Generate();
		LoopControlAST(Token tok):tok(tok)
		{}
	};
	/// BinaryExprAST - Expression class for a binary operator.
	class BinaryExprAST : public ExprAST {
		FunctionAST* func=nullptr;
	public:
		std::unique_ptr<ExprAST> LHS, RHS;
		Token Op;
		BinaryExprAST(Token Op, std::unique_ptr<ExprAST>&& LHS,
			std::unique_ptr<ExprAST>&& RHS)
			: Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
		BinaryExprAST(Token Op, std::unique_ptr<ExprAST>&& LHS,
			std::unique_ptr<ExprAST>&& RHS, SourcePos pos)
			: Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {
			Pos = pos;
		}

		void Phase1();
		llvm::Value* Generate();
		void print(int level) {
			ExprAST::print(level);
			std::cout << "OP:" << GetTokenString(Op) << "\n";
			LHS->print(level + 1);
			formatprint(level + 1); std::cout << "----------------\n";
			RHS->print(level + 1);
		}
	};
	/// BinaryExprAST - Expression class for a binary operator.
	class IndexExprAST : public ExprAST {
		std::unique_ptr<ExprAST> Expr, Index;
	public:
		void Phase1();
		llvm::Value* Generate();
		llvm::Value* GetLValue(bool checkHas) override;
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

	class AddressOfExprAST : public ExprAST {
		std::unique_ptr<ExprAST> expr;
		bool is_address_of;
	public:
		void Phase1();
		llvm::Value* Generate();
		AddressOfExprAST(unique_ptr<ExprAST>&& Expr, bool is_address_of, SourcePos Pos)
			: expr(std::move(Expr)), is_address_of(is_address_of){
			this->Pos = Pos;
		}
		void print(int level) {
			ExprAST::print(level);
			std::cout << "AddressOf\n";
			expr->print(level + 1);
			
		}
	};
	/// CallExprAST - Expression class for function calls.
	class CallExprAST : public ExprAST {
		std::unique_ptr<ExprAST> Callee;
		std::vector<std::unique_ptr<ExprAST>> Args;

	public:
		void Phase1();
		llvm::Value* Generate();
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
		llvm::Value* llvm_value = nullptr;
		ResolvedType resolved_type;
		llvm::Value* Generate();
		void PreGenerateForGlobal();
		void PreGenerateExternForGlobal(const string& package_name);
		void PreGenerateForArgument(llvm::Value* init,int argno);

		void Phase1();
		//parse the varible as a member of a class, will not add to the basic block environment
		void Phase1InClass();
		void move(unique_ptr<VariableDefAST>&& current,
			std::function<void(unique_ptr<VariableSingleDefAST>&&)> func)
		{
			func(dynamic_unique_ptr_cast<VariableSingleDefAST, VariableDefAST>(std::move(current)));
		}
		std::string name;

		//just resolve the type of the variable
		void Phase0()
		{
			if (resolved_type.isResolved())
				return;
			resolved_type = ResolvedType(*type,Pos);
		}

		VariableSingleDefAST(const std::string& _name, const ResolvedType& _type) : name(_name), resolved_type(_type), val(nullptr) {}
		VariableSingleDefAST(const std::string& _name, std::unique_ptr<Type>&& _type, std::unique_ptr<ExprAST>&& _val) : name(_name), type(std::move(_type)), val(std::move(_val)) {}
		VariableSingleDefAST(const std::string& _name, std::unique_ptr<Type>&& _type, std::unique_ptr<ExprAST>&& _val, SourcePos Pos) : name(_name), type(std::move(_type)), val(std::move(_val)) {
			this->Pos = Pos;
		}
		void print(int level) {
			VariableDefAST::print(level);
			std::cout << "Variable:" << name << " Type: "<< resolved_type.GetString()<< "\n";
			if (val)
				val->print(level + 1);
		}
	};

	class VariableMultiDefAST : public VariableDefAST {

	public:
		void Phase1()
		{
			for (auto& a : lst)
				a->Phase1();
		}
		llvm::Value* Generate();
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

	class LocalVarExprAST : public ResolvedIdentifierExprAST {
		VariableSingleDefAST* def;
	public:
		bool isMutable() { return true; }
		LocalVarExprAST(VariableSingleDefAST* def, SourcePos pos) :def(def) { Pos = pos; Phase1(); }
		void Phase1();
		llvm::Value* Generate();
		llvm::Value* GetLValue(bool checkHas) override { return checkHas? (llvm::Value*)1:def->llvm_value; };
	};

	/// PrototypeAST - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes).
	class PrototypeAST {
	protected:
		std::string Name;
		std::unique_ptr<VariableDefAST> Args;
		std::unique_ptr<Type> RetType;
		SourcePos pos;
		
	public:
		ClassAST * cls;
		//the index in CompileUnit.imported_module_names
		//if -1, means it is not imported from other modules
		int prefix_idx=-1;
		friend bool operator == (const PrototypeAST&, const PrototypeAST&);
		ResolvedType resolved_type;
		vector<unique_ptr<VariableSingleDefAST>> resolved_args;
		llvm::FunctionType* GenerateFunctionType();
		llvm::DIType* GenerateDebugType();
		//Put the definitions of arguments into a vector
		//resolve the types in the arguments and the returned value 
		void Phase0()
		{
			if (resolved_type.isResolved()) //if we have already resolved the type
				return;
			auto args = Args.get();
			if (args)
			{
				vector<unique_ptr<VariableSingleDefAST>>& resolved_args = this->resolved_args;
				args->move(std::move(Args), [&resolved_args](unique_ptr<VariableSingleDefAST>&& arg) {
					arg->Phase0();
					resolved_args.push_back(std::move(arg));
				});
			}
			resolved_type = ResolvedType(*RetType,pos);
		}
		void Phase1()
		{
			Phase0();
			for (auto&& dim : resolved_args)
			{
				dim->Phase1();
			}
		}
		PrototypeAST(const std::string &Name, vector<unique_ptr<VariableSingleDefAST>>&& ResolvedArgs, const ResolvedType& ResolvedType, ClassAST* cls, int prefix_idx)
			: Name(Name), Args(nullptr), RetType(nullptr), cls(cls), pos(0,0), resolved_args(std::move(ResolvedArgs)), resolved_type(ResolvedType),prefix_idx(prefix_idx){}
		PrototypeAST(const std::string &Name, std::unique_ptr<VariableDefAST>&& Args, std::unique_ptr<Type>&& RetType,ClassAST* cls,SourcePos pos)
			: Name(Name), Args(std::move(Args)), RetType(std::move(RetType)),pos(pos),cls(cls) {}

		const std::string &GetName() const { return Name; }
		void print(int level)
		{
			formatprint(level);
			std::cout << "Function Proto: " << Name << std::endl;
			for (auto& arg : resolved_args)
			{
				arg->print(level + 1);
			}
			formatprint(level + 1); std::cout << "Return type: "<<resolved_type.GetString()<< "\n";
		}
	};

	struct TemplateParameters
	{
		//the struct for TemplateParameter: if type is null,
		// then this parameter is a "typename" parameter. If 
		//not null, it is a constant parameter
		struct Parameter
		{
			unique_ptr<Type> type;
			string name;
			Parameter(unique_ptr<Type>&& type, const string& name) :type(std::move(type)), name(name) {}
		};
		vector<Parameter> params;
		TemplateParameters(vector<Parameter>&& params) : params(std::move(params)) {};
		TemplateParameters() {}
	};

	/// FunctionAST - This class represents a function definition itself.
	class FunctionAST : public ExprAST {
		ASTBasicBlock Body;
	public:
		TemplateParameters template_param;
		bool isDeclare;
		bool isImported=false;
		string link_name;
		std::unique_ptr<PrototypeAST> Proto;
		llvm::Function* llvm_func=nullptr;
		llvm::DIType* PreGenerate();
		llvm::Value* Generate();
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
			ASTBasicBlock&& Body)
			: Proto(std::move(Proto)), Body(std::move(Body)), isDeclare(false){}
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
			ASTBasicBlock&& Body, TemplateParameters&& template_param, SourcePos pos)
			: Proto(std::move(Proto)), Body(std::move(Body)), template_param(std::move(template_param)), isDeclare(false) {
			Pos = pos;
		}
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,const string& link_name , SourcePos pos)
			: Proto(std::move(Proto)), isDeclare(true),link_name(link_name) {
			Pos = pos;
		}

		//imported function
		FunctionAST(std::unique_ptr<PrototypeAST> Proto)
			: Proto(std::move(Proto)), isDeclare(true), isImported(true) {
		}

		//resolve the types of the argument and the returned value
		//put a phase0 because we may reference a function before we parse the function in phase1
		void Phase0()
		{
			if (resolved_type.isResolved())
				return;
			resolved_type.type = tok_func;
			resolved_type.index_level = 0;
			resolved_type.proto_ast = Proto.get();
			Proto->Phase0();
		}
		void Phase1();

		const string& GetName()
		{
			return Proto->GetName();
		}
		void print(int level)
		{
			ExprAST::print(level);
			if (isDeclare)
				std::cout << "extern ";
			std::cout << "Function def\n";
			Proto->print(level + 1);
			Body.print(level + 1);
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
		int index;
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
		FieldDef(AccessModifier access, std::unique_ptr<VariableSingleDefAST>&& decl,int index) :access(access), decl(std::move(decl)),index(index) {}
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

	class NewExprAST : public ExprAST {
		std::unique_ptr<Type> ty;
		string method;
		vector<std::unique_ptr<ExprAST>> args;
		MemberFunctionDef* func = nullptr;
	public:
		void Phase1();
		llvm::Value* Generate();
		NewExprAST(std::unique_ptr<Type>&& ty, vector<std::unique_ptr<ExprAST>>&& args, const string& method, SourcePos Pos)
			: ty(std::move(ty)), args(std::move(args)), method(method) {
			this->Pos = Pos;
		}
		void print(int level) {
			ExprAST::print(level);
			std::cout << "New\n";
			ty->print(level + 1);

		}
	};

	class ClassAST : public StatementAST {
	public:
		llvm::Value* Generate();
		std::string name;
		std::vector<FieldDef> fields;
		std::vector<MemberFunctionDef> funcs;
		unordered_map<reference_wrapper<const string>, int> fieldmap;
		unordered_map<reference_wrapper<const string>, int> funcmap;
		//if the class is imported from other package, this field will be the index in cu.imported_module_names
		//if the class is defined in the current package, this field will be -1
		//if the class is an orphan class, this field will be -2
		int package_name_idx=-1;
		llvm::StructType* llvm_type=nullptr;
		llvm::DIType* llvm_dtype = nullptr;
		ClassAST(const std::string& name,
			SourcePos pos)
			: name(name) {
			Pos = pos;
		}

		string GetUniqueName()
		{
			if (package_name_idx == -1)
				return cu.symbol_prefix + name;
			else if (package_name_idx == -2)
				return name;
			else
				return cu.imported_module_names[package_name_idx] + '.' + name;
		}
		void PreGenerate();
		void PreGenerateFuncs();

		//run phase0 in all members
		void Phase0()
		{
			for (auto& funcdef : funcs)
			{
				funcdef.decl->Phase0();
			}
			for (auto& fielddef : fields)
			{
				fielddef.decl->Phase0();
			}
		}
		void Phase1();

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


	/// MemberExprAST - Expression class for function calls.
	class MemberExprAST : public ResolvedIdentifierExprAST {
		std::unique_ptr<ExprAST> Obj;
		std::string member;
		union
		{
			MemberFunctionDef* func;
			FieldDef* field;
			FunctionAST* import_func;
			VariableSingleDefAST* import_dim;
		};
	public:
		llvm::Value* llvm_obj = nullptr;
		llvm::Value* Generate();
		llvm::Value* GetLValue(bool checkHas) override;
		enum
		{
			member_error,
			member_package,
			member_field,
			member_function,
			member_imported_dim,
			member_imported_function,
		}kind;
		void Phase1();
		bool isMutable() {
			return kind == member_field || kind==member_imported_dim;
		}
		void print(int level)
		{
			ExprAST::print(level); std::cout << "Member\n";
			Obj->print(level + 1);
			formatprint(level + 1); std::cout << "---------\n";
			formatprint(level + 1); std::cout << member << "\n";

		}
		MemberExprAST(std::unique_ptr<ExprAST> &&Obj,
			const std::string& member)
			: Obj(std::move(Obj)), member(member), kind(member_error), func(nullptr) {}
		MemberExprAST(std::unique_ptr<ExprAST> &&Obj,
			MemberFunctionDef* member,SourcePos pos)
			: Obj(std::move(Obj)), func(member), kind(member_function) {
			resolved_type = func->decl->resolved_type;
			Pos = pos;
		}
		MemberExprAST(std::unique_ptr<ExprAST> &&Obj,
			FieldDef* member, SourcePos pos)
			: Obj(std::move(Obj)), field(member), kind(member_field) {
			resolved_type = field->decl->resolved_type;
			Pos = pos;
		}

	};

}