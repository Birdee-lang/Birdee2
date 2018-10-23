#pragma once

#include <memory>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <map>
#include <functional>
#include "SourcePos.h"
#include "Util.h"
#include "TokenDef.h"
#include <LibDef.h>
#include <assert.h>

namespace Birdee
{
	struct TemplateArgument;
}
namespace std
{
	inline bool operator < (reference_wrapper<const vector<Birdee::TemplateArgument>> a, reference_wrapper<const vector<Birdee::TemplateArgument>> b)
	{
		return a.get() < b.get();
	}

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
	using std::map;
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

	BD_CORE_API extern SourcePos GetCurrentSourcePos();
	BD_CORE_API extern string GetTokenString(Token tok);

	struct ImportedModule
	{
		unordered_map<string, unique_ptr<ClassAST>> classmap;
		unordered_map<string, unique_ptr<FunctionAST>> funcmap;
		unordered_map<string, unique_ptr<VariableSingleDefAST>> dimmap;

		unordered_map<std::reference_wrapper<const string>, ClassAST*> imported_classmap;
		unordered_map<std::reference_wrapper<const string>, FunctionAST*> imported_funcmap;
		unordered_map<std::reference_wrapper<const string>, VariableSingleDefAST*> imported_dimmap;

		vector<vector<string>> user_imports;
		BD_CORE_API void HandleImport();
		BD_CORE_API void Init(const vector<string>& package,const string& module_name);
	};

	//a quick structure to find & store names of imported packages
	class BD_CORE_API ImportTree
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

	class BD_CORE_API Type {

	public:
		Token type;
		int index_level = 0;
		virtual ~Type() = default;
		virtual unique_ptr<Type> Copy();
		bool operator < (const Type& that);
		Type(Token _type) :type(_type) {}
		virtual void print(int level)
		{
			formatprint(level);
			std::cout << "Type " << type << " index: " << index_level;
		}
	};

	class ExprAST;
	class BD_CORE_API GeneralIdentifierType :public Type {
	public:
		unique_ptr<vector<unique_ptr<ExprAST>>> template_args;
		GeneralIdentifierType(Token tok) :Type(tok) {}
	};

	class BD_CORE_API ScriptType :public Type {

	public:
		virtual unique_ptr<Type> Copy();
		std::string script;
		ScriptType(const std::string& _script) :Type(tok_script), script(_script) {}
		void print(int level)
		{
			Type::print(level);
			std::cout << " script: " << script;
		}
	};


	class BD_CORE_API IdentifierType :public GeneralIdentifierType {
		
	public:
		virtual unique_ptr<Type> Copy();
		std::string name;
		IdentifierType(const std::string&_name) :GeneralIdentifierType(tok_identifier), name(_name) {}
		void print(int level)
		{
			Type::print(level);
			std::cout << " Name: " << name;
		}
	};

	class BD_CORE_API QualifiedIdentifierType :public GeneralIdentifierType {

	public:
		virtual unique_ptr<Type> Copy();
		vector<string> name;
		QualifiedIdentifierType(vector<string>&&_name) :GeneralIdentifierType(tok_package), name(std::move(_name)) {}
		void print(int level)
		{
			Type::print(level);
			std::cout << " QualifiedName: " << name.back();
		}
	};

	class ClassAST;
	class PrototypeAST;
	BD_CORE_API extern string GetClassASTName(ClassAST*);
	BD_CORE_API extern bool operator ==(const PrototypeAST& ,const PrototypeAST& );
	class BD_CORE_API ResolvedType {
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
		string GetString() const;
		bool operator < (const ResolvedType& other) const;
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
	class BD_CORE_API StatementAST {
	public:
		virtual Value* Generate()=0;
		SourcePos Pos=GetCurrentSourcePos();
		virtual void Phase1() = 0;
		virtual ~StatementAST() = default;
		virtual unique_ptr<StatementAST> Copy()=0;
		virtual void print(int level) {
			for (int i = 0; i < level; i++)
				std::cout << "\t";
		}
	};

	class BD_CORE_API ExprAST : public StatementAST {
	public:
		ResolvedType resolved_type;
		virtual llvm::Value* GetLValue(bool checkHas) { return nullptr; };
		virtual ~ExprAST() = default;
		void print(int level) {
			StatementAST::print(level); std::cout << "Type: "<< resolved_type.GetString()<<" ";
		}
	};
	class BD_CORE_API AnnotationStatementAST : public StatementAST {
	public:
		std::vector<std::string> anno;
		unique_ptr< StatementAST> impl;
		AnnotationStatementAST(vector<string>&& anno, unique_ptr< StatementAST>&& impl)
			:anno(std::move(anno)), impl(std::move(impl)) {}
		virtual Value* Generate();
		virtual void Phase1();
		unique_ptr<StatementAST> Copy();
	};

	class BD_CORE_API CompileUnit
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
		bool is_print_ir = false;
		bool expose_main = false;
		bool is_script_mode = false;
		bool is_compiler_mode = false; //if the Birdee Compiler Core is called by birdeec, it should be set true; if called & loaded by python, remain false
		vector<string> search_path; //the search paths, with "/" at the end
		vector<unique_ptr<StatementAST>> toplevel;
		unordered_map<std::reference_wrapper<const string>, std::reference_wrapper<ClassAST>> classmap;
		unordered_map<std::reference_wrapper<const string>, std::reference_wrapper<FunctionAST>> funcmap;
		unordered_map<std::reference_wrapper<const string>, std::reference_wrapper<VariableSingleDefAST>> dimmap;

		//maps from short names ("import a.b:c" => "c") to the imported AST
		unordered_map<std::reference_wrapper<const string>, ClassAST*> imported_classmap;
		unordered_map<std::reference_wrapper<const string>, FunctionAST*> imported_funcmap;
		unordered_map<std::reference_wrapper<const string>, VariableSingleDefAST*> imported_dimmap;

		vector<ClassAST*> imported_class_templates;
		vector<FunctionAST*> imported_func_templates;
		/*
		The classes that are referenced by an imported class, but the packages of the 
		referenced classes are not yet imported. The mapping from qualified names of classes to AST
		*/
		map<string, unique_ptr<ClassAST>> orphan_class;

		vector<string> imported_module_names;

		//all imports by the source code. if imports[X].back() is "*" or starts with ":", it is a name import
		vector<vector<string>> imports;
		/*
			for quick module name-looking. We need this because we need to quickly distinguish in "AAA.BBB",
			whether AAA is a variable name or a package name.
		*/
		ImportTree imported_packages;
		void Clear();
		void Phase0();
		void Phase1();
		void InitForGenerate();
		void Generate();
		void AbortGenerate();
	};
	BD_CORE_API extern  CompileUnit cu;

	class BD_CORE_API ResolvedIdentifierExprAST : public ExprAST {
	public:
		virtual bool isMutable() = 0;
	};
	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class BD_CORE_API NumberExprAST : public ResolvedIdentifierExprAST {
	public:
		NumberLiteral Val;
		bool isMutable() { return false; } //for template argument replacement
		Value* Generate();
		virtual void Phase1();
		NumberExprAST(const NumberLiteral& Val) : Val(Val) {}
		void ToString(std::ostream& os)
		{
			switch (Val.type)
			{
			case tok_byte:
				os << "const byte " << Val.v_int ;
				break;
			case tok_int:
				os << "const int " << Val.v_int ;
				break;
			case tok_long:
				os << "const long " << Val.v_long ;
				break;
			case tok_uint:
				os << "const uint " << Val.v_uint;
				break;
			case tok_ulong:
				os << "const ulong " << Val.v_ulong;
				break;
			case tok_float:
				os << "const float " << Val.v_double;
				break;
			case tok_double:
				os << "const double " << Val.v_double ;
				break;
			}
		}
		void print(int level) {
			ExprAST::print(level);
			ToString(std::cout);
			std::cout << "\n";
		}
		unique_ptr<StatementAST> Copy();
	};

	class BD_CORE_API BasicTypeExprAST : public ExprAST
	{
	public:
		unique_ptr<Type> type;
		Value * Generate();
		virtual void Phase1() {}
		BasicTypeExprAST() {}
		BasicTypeExprAST(Token token, SourcePos pos) { Pos = pos; type = make_unique<Type>(token); };
		void print(int level) {
			ExprAST::print(level); std::cout << "BasicType"<<type->type<<"\n";
		}
		unique_ptr<StatementAST> Copy();
	};

	class PrototypeAST;
	class BD_CORE_API ReturnAST : public StatementAST {
	public:
		std::unique_ptr<ExprAST> Val;
		Value* Generate();
		virtual void Phase1();
		ReturnAST(std::unique_ptr<ExprAST>&& val, SourcePos pos) : Val(std::move(val)){ Pos = pos; };
		void print(int level) {
			StatementAST::print(level); std::cout << "Return\n";
			Val->print(level + 1);
		}
		unique_ptr<StatementAST> Copy();
	};

	class BD_CORE_API StringLiteralAST : public ResolvedIdentifierExprAST {
	public:
		bool isMutable() { return false; }//for template argument replacement
		std::string Val;
		virtual void Phase1();
		llvm::Value* Generate();
		StringLiteralAST(const std::string& Val) : Val(Val) {}
		void print(int level) { ExprAST::print(level); std::cout << "\"" << Val << "\"\n"; }
		unique_ptr<StatementAST> Copy();
	};

	/// IdentifierExprAST - Expression class for referencing a variable, like "a".
	class BD_CORE_API IdentifierExprAST : public ExprAST {
	public:
		std::string Name;
		unique_ptr<ResolvedIdentifierExprAST> impl;
		void Phase1();
		llvm::Value * GetLValue(bool checkHas) override { return impl->GetLValue(checkHas); };
		llvm::Value* Generate();
		IdentifierExprAST(const std::string &Name) : Name(Name) {}
		void print(int level) { ExprAST::print(level); std::cout << "Identifier:" << Name << "\n"; }
		std::unique_ptr<StatementAST> Copy();
	};

	class BD_CORE_API ResolvedFuncExprAST : public ResolvedIdentifierExprAST {
	public:
		FunctionAST * def;
		bool isMutable() { return false; }
		ResolvedFuncExprAST(FunctionAST* def, SourcePos pos) :def(def) { Phase1(); Pos = pos; }
		void Phase1();
		llvm::Value* Generate();
		std::unique_ptr<StatementAST> Copy();
	};

	class BD_CORE_API ThisExprAST : public ExprAST {
	public:
		void Phase1();
		llvm::Value* Generate();
		ThisExprAST()   {}
		std::unique_ptr<StatementAST> Copy();
		ThisExprAST(ClassAST* cls, SourcePos pos) { resolved_type.type = tok_class; resolved_type.class_ast = cls; Pos = pos; }
		void print(int level) { ExprAST::print(level); std::cout << "this" << "\n"; }
	};
	class BD_CORE_API BoolLiteralExprAST : public ExprAST {
	public:
		bool v;
		void Phase1() {};
		llvm::Value* Generate();
		std::unique_ptr<StatementAST> Copy();
		BoolLiteralExprAST(bool v) : v(v) { resolved_type.type = tok_boolean;}
		void print(int level) { ExprAST::print(level); std::cout << "bool" << "\n"; }
	};

	class BD_CORE_API NullExprAST : public ExprAST {
	public:
		void Phase1() {};
		llvm::Value* Generate();
		std::unique_ptr<StatementAST> Copy();
		NullExprAST() { resolved_type.type = tok_null; }
		void print(int level) { ExprAST::print(level); std::cout << "null" << "\n"; }
	};
	class ASTBasicBlock
	{
	public:
		std::vector<std::unique_ptr<StatementAST>> body;
		BD_CORE_API ASTBasicBlock Copy();
		BD_CORE_API void Phase1();
		BD_CORE_API void Phase1(PrototypeAST* proto);
		BD_CORE_API bool Generate();
		void print(int level)
		{
			for (auto&& s : body)
				s->print(level);
		}
	};

	/// IfBlockAST - Expression class for If block.
	class BD_CORE_API IfBlockAST : public StatementAST {
	public:
		std::unique_ptr<ExprAST> cond;
		ASTBasicBlock iftrue;
		ASTBasicBlock iffalse;
		void Phase1();
		std::unique_ptr<StatementAST> Copy();
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
	/// ForBlockAST - Expression class for For block.
	class BD_CORE_API ForBlockAST : public StatementAST {
	public:
		std::unique_ptr<StatementAST> init; //the code that initialize the loop variable
		ExprAST* loop_var; //the variable definition, if isdim, this member has no meaning
		std::unique_ptr<ExprAST> till; //the end of the loop
		bool including; //if inclusive --- "to" or "till"
		bool isdim; //if isdim, "init" will be a VariableSingleDefAST, and loop_var is meaningless.
		ASTBasicBlock block;
		std::unique_ptr<StatementAST> Copy();
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

	class BD_CORE_API WhileBlockAST : public StatementAST {
	public:
		std::unique_ptr<ExprAST> cond; //the end of the loop
		ASTBasicBlock block;
		std::unique_ptr<StatementAST> Copy();
		void Phase1();
		llvm::Value* Generate();
		WhileBlockAST(std::unique_ptr<ExprAST>&& cond,
			ASTBasicBlock&& block,
			SourcePos pos)
			: cond(std::move(cond)), block(std::move(block)) {
			Pos = pos;
		}

		void print(int level) {
			StatementAST::print(level);	std::cout << "While" << "\n";
			cond->print(level + 1);
			StatementAST::print(level + 1);	std::cout << "Block" << "\n";
			block.print(level + 2);
		}
	};

	class BD_CORE_API LoopControlAST :public StatementAST {
	public:
		Token tok;
		void Phase1() {};
		llvm::Value* Generate();
		LoopControlAST(Token tok):tok(tok)
		{}
		std::unique_ptr<StatementAST> Copy();
	};
	/// BinaryExprAST - Expression class for a binary operator.
	class BD_CORE_API BinaryExprAST : public ExprAST {
	public:
		FunctionAST* func = nullptr;
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
		std::unique_ptr<StatementAST> Copy();
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
	class FunctionTemplateInstanceExprAST;
	class BD_CORE_API IndexExprAST : public ExprAST {
	public:
		std::unique_ptr<ExprAST> Expr, Index;
		unique_ptr<FunctionTemplateInstanceExprAST> instance;
		void Phase1();
		std::unique_ptr<StatementAST> Copy();
		bool isTemplateInstance();
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
	struct BD_CORE_API TemplateArgument
	{
		enum TemplateArgumentType {
			TEMPLATE_ARG_TYPE,
			TEMPLATE_ARG_EXPR
		}kind;
		//TemplateArgument(const TemplateArgument&) = delete;
		//TemplateArgument& operator = (const TemplateArgument&) = delete;
		unique_ptr<ExprAST> expr; //fix-me: should use union?
		ResolvedType type;
		TemplateArgument Copy() const;
		TemplateArgument(ResolvedType type) :kind(TEMPLATE_ARG_TYPE),type(type), expr(nullptr){}
		TemplateArgument(unique_ptr<ExprAST>&& expr) :kind(TEMPLATE_ARG_EXPR),expr(std::move(expr)),type(nullptr) {}
		TemplateArgument(TemplateArgument&& other) :kind(other.kind), expr(std::move(other.expr)), type(other.type) {}
		TemplateArgument& operator = (TemplateArgument&&) = default;
		bool operator < (const TemplateArgument&) const;
	};

	class BD_CORE_API FunctionTemplateInstanceExprAST : public ExprAST {
	public:
		FunctionAST* instance = nullptr;
		vector<unique_ptr<ExprAST>> raw_template_args;
		vector<TemplateArgument> template_args;
		std::unique_ptr<ExprAST> expr;
		void Phase1();
		std::unique_ptr<StatementAST> Copy();
		llvm::Value* Generate();
		llvm::Value* GetLValue(bool checkHas) override { return nullptr; };
		FunctionTemplateInstanceExprAST(std::unique_ptr<ExprAST>&& expr,
			vector<unique_ptr<ExprAST>>&& raw_template_args, SourcePos Pos)
			: expr(std::move(expr)), raw_template_args(std::move(raw_template_args)) {
			this->Pos = Pos;
		}
		void print(int level) {
			ExprAST::print(level);
			std::cout << "FunctionTemplateInstanceExprAST\n";
			expr->print(level + 1);
			ExprAST::print(level + 1); std::cout << "----------------\n";
		}
	};
	class BD_CORE_API AddressOfExprAST : public ExprAST {
	public:
		std::unique_ptr<ExprAST> expr;
		bool is_address_of; //true for addressof, false for pointerof
		void Phase1();
		std::unique_ptr<StatementAST> Copy();
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
	class BD_CORE_API CallExprAST : public ExprAST {
	public:
		std::unique_ptr<ExprAST> Callee;
		std::vector<std::unique_ptr<ExprAST>> Args;
		std::unique_ptr<StatementAST> Copy();
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

	class BD_CORE_API VariableDefAST : public StatementAST {
	public:
		virtual void move(unique_ptr<VariableDefAST>&& current,
			std::function<void(unique_ptr<VariableSingleDefAST>&&)> func) = 0;
	};



	class BD_CORE_API VariableSingleDefAST : public VariableDefAST {
		std::unique_ptr<Type> type;
	public:
		std::unique_ptr<ExprAST> val;
		llvm::Value* llvm_value = nullptr;
		ResolvedType resolved_type;
		llvm::Value* Generate();
		void PreGenerateForGlobal();
		void PreGenerateExternForGlobal(const string& package_name);
		void PreGenerateForArgument(llvm::Value* init,int argno);
		std::unique_ptr<StatementAST> Copy();
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
		BD_CORE_API void Phase1()
		{
			for (auto& a : lst)
				a->Phase1();
		}
		BD_CORE_API llvm::Value* Generate();
		std::unique_ptr<StatementAST> Copy();
		std::vector<std::unique_ptr<VariableSingleDefAST>> lst;
		BD_CORE_API VariableMultiDefAST(std::vector<std::unique_ptr<VariableSingleDefAST>>&& vec) :lst(std::move(vec)) {}
		BD_CORE_API VariableMultiDefAST(std::vector<std::unique_ptr<VariableSingleDefAST>>&& vec, SourcePos pos) :lst(std::move(vec)) {
			Pos = pos;
		}
		BD_CORE_API void move(unique_ptr<VariableDefAST>&& current,
			std::function<void(unique_ptr<VariableSingleDefAST>&&)> func)
		{
			for (auto&& var : lst)
				func(std::move(var));
		}

		BD_CORE_API void print(int level) {
			//VariableDefAST::print(level);
			for (auto& a : lst)
				a->print(level);
		}
	};

	class BD_CORE_API LocalVarExprAST : public ResolvedIdentifierExprAST {
	public:
		VariableSingleDefAST* def;
		std::unique_ptr<StatementAST> Copy();
		bool isMutable() { return true; }
		LocalVarExprAST(VariableSingleDefAST* def, SourcePos pos) :def(def) { Pos = pos; Phase1(); }
		void Phase1();
		llvm::Value* Generate();
		llvm::Value* GetLValue(bool checkHas) override { return checkHas? (llvm::Value*)1:def->llvm_value; };
	};

	/// PrototypeAST - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes).
	class BD_CORE_API PrototypeAST {
	protected:
		
		std::unique_ptr<VariableDefAST> Args;
		std::unique_ptr<Type> RetType;
		SourcePos pos;
		
	public:
		std::string Name;
		std::unique_ptr<PrototypeAST> Copy();
		ClassAST * cls;
		//the index in CompileUnit.imported_module_names
		//if -1, means it is not imported from other modules
		int prefix_idx=-1;
		friend BD_CORE_API bool operator == (const PrototypeAST&, const PrototypeAST&);
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
			: Name(Name), Args(nullptr), RetType(nullptr), cls(cls), pos(0,0,0), resolved_args(std::move(ResolvedArgs)), resolved_type(ResolvedType),prefix_idx(prefix_idx){}
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

	//the struct for TemplateParameter: if type is null,
	// then this parameter is a "typename" parameter. If 
	//not null, it is a constant parameter
	struct TemplateParameter
	{
		unique_ptr<Type> type;
		string name;
		bool isTypeParameter() const { return type == nullptr; }
		TemplateParameter(unique_ptr<Type>&& type, const string& name) :type(std::move(type)), name(name) {}
		TemplateParameter(TemplateParameter&& other) :type(std::move(other.type)), name(std::move(other.name)) {}
		TemplateParameter& operator = (TemplateParameter&& other)
		{
			type = std::move(other.type); 
			name = std::move(other.name);
			return *this;
		}
	};

	struct SourceStringHolder
	{
		enum
		{
			HOLDER_STRING,
			HOLDER_STRING_VIEW
		}type;

		string heldstr;
		union
		{
			struct {
				uint32_t start;
				uint32_t len;
			}view;
		};
		void set(string&& str)
		{
			type = HOLDER_STRING;
			heldstr = std::move(str);
		}
		void set(const string& prefix, uint32_t start, uint32_t len)
		{
			type = HOLDER_STRING_VIEW;
			heldstr = prefix;
			view.start = start; view.len = len;
		}
		SourceStringHolder()
		{
			set(string());
		}
		const string& get() const
		{
			assert(type == HOLDER_STRING);
			return heldstr;
		}
		string get(const string& orig)
		{
			assert(type == HOLDER_STRING_VIEW);
			return heldstr + orig.substr(view.start,view.len);
		}
		bool empty()
		{
			return heldstr.empty();
		}
	};
	template<typename T>
	struct TemplateParameters
	{

		vector<TemplateParameter> params;
		map<reference_wrapper<const vector<TemplateArgument>>, unique_ptr<T>> instances;
		SourceStringHolder source; //no need to copy this field
		ImportedModule* mod = nullptr;
		/*
		For classast, it will take the ownership of v. For FunctionAST, it won't
		*/
		BD_CORE_API T* GetOrCreate(vector<TemplateArgument>* v, T* source_template, SourcePos pos);
		T* Get(vector<TemplateArgument>& v)
		{
			auto f = instances.find(v);
			if (f == instances.end())
				return nullptr;
			return f->second.get();
		}
		void AddImpl(vector<TemplateArgument>& v, unique_ptr<T> impl)
		{
			assert(instances.find(v) == instances.end());
			instances.insert(std::make_pair(reference_wrapper<const vector<TemplateArgument>>(v), std::move(impl)));
		}
		TemplateParameters(vector<TemplateParameter>&& params) : params(std::move(params)){};
		//TemplateParameters(vector<TemplateParameter>&& params, string&& src) : params(std::move(params)), source(std::move(src)){};
		TemplateParameters() {}
		BD_CORE_API unique_ptr<TemplateParameters<T>> Copy();
		BD_CORE_API void ValidateArguments(const vector<TemplateArgument>& args, SourcePos Pos);
	};

	/// FunctionAST - This class represents a function definition itself.
	class BD_CORE_API FunctionAST : public ExprAST {
	public:
		ASTBasicBlock Body;
		unique_ptr<TemplateParameters<FunctionAST>> template_param;
		bool isDeclare;
		bool isTemplateInstance = false;
		bool isImported=false;
		string link_name;
		std::unique_ptr<PrototypeAST> Proto;
		llvm::Function* llvm_func=nullptr;
		llvm::DIType* PreGenerate();
		llvm::Value* Generate();
		std::unique_ptr<StatementAST> Copy();
		std::unique_ptr<FunctionAST> CopyNoTemplate();
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
			ASTBasicBlock&& Body)
			: Proto(std::move(Proto)), Body(std::move(Body)), isDeclare(false){}
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
			ASTBasicBlock&& Body, unique_ptr<TemplateParameters<FunctionAST>>&& template_param, SourcePos pos)
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
			if (isTemplate())
				return;
			if (resolved_type.isResolved())
				return;
			resolved_type.type = tok_func;
			resolved_type.index_level = 0;
			resolved_type.proto_ast = Proto.get();
			Proto->Phase0();
		}
		void Phase1();

		bool isTemplate() { return template_param!=nullptr && template_param->params.size() != 0; }

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
	class BD_CORE_API FieldDef
	{
	public:
		int index;
		AccessModifier access;
		std::unique_ptr<VariableSingleDefAST> decl;
		FieldDef Copy();
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
	class BD_CORE_API MemberFunctionDef
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
		MemberFunctionDef Copy();
		MemberFunctionDef(AccessModifier access, std::unique_ptr<FunctionAST>&& decl) :access(access), decl(std::move(decl)) {}
	};

	class BD_CORE_API NewExprAST : public ExprAST {
		std::unique_ptr<Type> ty;
		string method;
	public:
		vector<std::unique_ptr<ExprAST>> args;
		MemberFunctionDef* func = nullptr;
		std::unique_ptr<StatementAST> Copy();
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

	class BD_CORE_API ClassAST : public StatementAST {
	public:
		std::unique_ptr<StatementAST> Copy();
		std::unique_ptr<ClassAST> CopyNoTemplate();
		llvm::Value* Generate();
		std::string name;
		std::vector<FieldDef> fields;
		std::vector<MemberFunctionDef> funcs;
		unique_ptr< vector<TemplateArgument>> template_instance_args;
		ClassAST* template_source_class = nullptr;
		unique_ptr<TemplateParameters<ClassAST>> template_param;
		unordered_map<reference_wrapper<const string>, int> fieldmap;
		unordered_map<reference_wrapper<const string>, int> funcmap;
		//if the class is imported from other package, this field will be the index in cu.imported_module_names
		//if the class is defined in the current package, this field will be -1
		//if the class is an orphan class, this field will be -2
		//if package_name_idx is -3, the ClassAST is a placeholder
		int package_name_idx=-1;
		llvm::StructType* llvm_type=nullptr;
		llvm::DIType* llvm_dtype = nullptr;
		ClassAST(const std::string& name,
			SourcePos pos)
			: name(name) {
			Pos = pos;
		}
		bool isTemplateInstance() { return template_instance_args != nullptr; }
		bool isTemplate() { return template_param != nullptr && template_param->params.size() != 0; }
		string GetUniqueName();
		void PreGenerate();
		void PreGenerateFuncs();

		//run phase0 in all members
		void Phase0()
		{
			if (isTemplate())
				return;
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
	class BD_CORE_API MemberExprAST : public ResolvedIdentifierExprAST {
		std::string member;
	public:
		std::unique_ptr<ExprAST> Obj;
		vector<string> ToStringArray();
		std::unique_ptr<StatementAST> Copy();
		union
		{
			MemberFunctionDef* func;
			FieldDef* field;
			FunctionAST* import_func;
			VariableSingleDefAST* import_dim;
		};
		llvm::Value* llvm_obj = nullptr;
		llvm::Value* Generate();
		llvm::Value* GetLValue(bool checkHas) override;
		enum MemberType
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

	class BD_CORE_API ScriptAST : public ExprAST
	{
	public:
		unique_ptr<StatementAST> stmt;
		string script;
		virtual Value* Generate();
		virtual void Phase1();
		virtual unique_ptr<StatementAST> Copy();
		virtual void print(int level) {
			ExprAST::print(level);
			std::cout << "script " << script<<"\n";
		}
		ScriptAST(const string& str):script(str) {}
		virtual llvm::Value* GetLValue(bool checkHas);
	};

}

