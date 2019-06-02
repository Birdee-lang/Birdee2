
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include "Tokenizer.h"
#include <iostream>
#include <functional>
#include <unordered_map>
#include "BdAST.h"
#include "CompileError.h"
#include <assert.h>
#include <unordered_set>
#include <iostream>
#include "TemplateUtil.h"
using namespace Birdee;

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//




BD_CORE_API extern Tokenizer tokenizer;


namespace Birdee
{
	BD_CORE_API SourcePos GetCurrentSourcePos()
	{
		return SourcePos{ tokenizer.source_idx, tokenizer.GetLine(),tokenizer.GetPos() };
	}
	BD_CORE_API string GetTokenString(Token tok)
	{
		static std::map<Token, std::string> tok_names = {
			{ tok_mul,"*" },
		{ tok_div,"/" },
		{ tok_mod,"%" },
		{ tok_add,"+" },
		{ tok_minus,"-" },
		{ tok_lt,"<" },
		{ tok_gt,">" },
		{ tok_le,"<=" },
		{ tok_ge,">=" },
		{ tok_equal,"==" },
		{ tok_ne,"!=" },
		{ tok_cmp_equal,"===" },
		{ tok_and,"&" },
		{ tok_xor,"^" },
		{ tok_or,"|" },
		{ tok_logic_and,"&&" },
		{ tok_logic_or,"||" },
		{ tok_assign,"=" },
		{ tok_int, "int" },
		{ tok_long, "long" },
		{ tok_byte, "byte" },
		{ tok_ulong, "ulong" },
		{ tok_uint, "uint" },
		{ tok_float, "float" },
		{ tok_double, "double" },
		{ tok_boolean, "boolean" },
		{ tok_pointer,"pointer" },
		};
		return tok_names[tok];
	}
	
}

inline int DoWarn(const string& str)
{
	std::cerr << "Warning: " << str;
	return 0;
}
#define WarnAssert(_b,_msg) ( (_b) ? 0 : DoWarn(_msg));
#define CompileAssert(_b,_msg) ( (_b) ? 0 : (throw CompileError(tokenizer.GetSourcePos(), _msg),0) )
inline void CompileExpect(Token expected_tok, const std::string& msg)
{
	if (tokenizer.CurTok != expected_tok)
	{
		throw CompileError(tokenizer.GetSourcePos(), msg);
	}

	tokenizer.GetNextToken();
}


inline void CompileExpect(std::initializer_list<Token> expected_tok, const std::string& msg)
{

	for (Token t : expected_tok)
	{
		if (t == tokenizer.CurTok)
		{
			tokenizer.GetNextToken();
			return;
		}
	}
	throw CompileError(tokenizer.GetSourcePos(), msg);
}




inline void CompileCheckGlobalConflict(SourcePos pos, const std::string& name)
{
	auto prv_cls = cu.classmap.find(name);
	if (prv_cls != cu.classmap.end())
	{
		throw CompileError(pos, string("The global name ") + name + " is already defined in " + prv_cls->second.first->Pos.ToString());
	}
	auto prv_func = cu.funcmap.find(name);
	if (prv_func != cu.funcmap.end())
	{
		throw CompileError(pos, string("The global name ") + name + " is already defined in " + prv_func->second.first->Pos.ToString());
	}
	auto prv_dim = cu.dimmap.find(name);
	if (prv_dim != cu.dimmap.end())
	{
		throw CompileError(pos, string("The global name ") + name + " is already defined in " + prv_dim->second.first->Pos.ToString());
	}
}

BD_CORE_API Tokenizer SwitchTokenizer(Tokenizer&& tokzr)
{
	Tokenizer t = std::move(tokenizer);
	tokenizer = std::move(tokzr);
	return std::move(t);
}

/////////////////////////////////////////////////////////////////////////////////////
BD_CORE_API std::unique_ptr<ExprAST> ParseExpressionUnknown();
std::unique_ptr<FunctionAST> ParseFunction(ClassAST*);
std::unique_ptr<IfBlockAST> ParseIf();
////////////////////////////////////////////////////////////////////////////////////
PrototypeAST *current_func_proto = nullptr;
int unnamed_func_cnt = 0;

static unordered_map<string, std::function<void(StatementAST*,bool)>> interal_annontation_map = {
	{"stack_capture", [](StatementAST* stmt,bool is_top_level) {
		CompileAssert(isa<FunctionAST>(stmt),"The stack_capture can only be applied on functions");
		FunctionAST* func = static_cast<FunctionAST*>(stmt);
		func->capture_on_stack = true;
	}},
	{"init_script", [](StatementAST* stmt,bool is_top_level) {
		CompileAssert(is_top_level && isa<ScriptAST>(stmt),"The init_script can only be applied on top-level scripts");
		ScriptAST* func = static_cast<ScriptAST*>(stmt);
		cu.init_scripts.push_back(func);
	}},
	{"enable_rtti", [](StatementAST* stmt,bool is_top_level) {
		CompileAssert(isa<ClassAST>(stmt),"The init_script can only be applied on class definitions");
		ClassAST* cls = static_cast<ClassAST*>(stmt);
		cls->needs_rtti = true;
	}},
};

//for all annotation, find interal annotation and apply them. Comsume and remove all internal annotations 
static void ApplyInternalAnnotations(vector<string>& anno, StatementAST* stmt, bool is_top_level=false)
{
	for (auto itr = anno.begin(); itr != anno.end();)
	{
		auto mapitr = interal_annontation_map.find(*itr);
		if (mapitr != interal_annontation_map.end())
		{
			mapitr->second(stmt,is_top_level); //call the annotation function
			itr = anno.erase(itr);
		}
		else
		{
			itr++;
		}
	}
}

void ClearParserState()
{
	current_func_proto = nullptr;
	unnamed_func_cnt = 0;
}

void ParsePackageName(vector<string>& ret);

void ParseTemplateArgsForType(GeneralIdentifierType* type)
{
	if (tokenizer.CurTok == tok_left_index)
	{
		tokenizer.GetNextToken();
		if (tokenizer.CurTok == tok_right_index)
		{
			tokenizer.GetNextToken();
			type->index_level++;
			return;
		}
		type->template_args = make_unique<vector<unique_ptr<ExprAST>>>();
		type->template_args->push_back(ParseExpressionUnknown());
		while (tokenizer.CurTok == tok_comma)
		{
			tokenizer.GetNextToken();
			type->template_args->push_back(ParseExpressionUnknown());
		}
		CompileExpect(tok_right_index, "Expected ] after template arguments");		
	}
}

static void BasicBlockCheckUnknownToken(std::initializer_list<Token> expect)
{
	for (Token t : expect)
	{
		if (t == tokenizer.CurTok)
		{
			tokenizer.GetNextToken();
			return;
		}
	}
	auto tok = tokenizer.CurTok;
	string msg;
	switch (tok)
	{
	case tok_catch:
		msg = "The catch clause does not match any try clause";
		break;
	case tok_else:
		msg = "The else clause does not match any if clause";
		break;
	case tok_end:
		msg = "Unmatched \'end\'";
		break;
	default:
		msg = "Encountered unknown token. Expecting a new line after an expression";
	}
	throw CompileError(msg);
}

static std::unordered_set<Token> basic_types = { tok_byte,tok_int,tok_long,tok_ulong,tok_uint,tok_float,tok_double,tok_boolean,tok_pointer };
//parse basic type, may get the array type if there is a [ after GeneralIdentifierType (QualifiedIdentifierType/IdentifierType)
std::unique_ptr<Type> ParseBasicType()
{
	
	std::unique_ptr<Type> type;
	if (tokenizer.CurTok == tok_identifier)
	{
		string iden = tokenizer.IdentifierStr;
		Token next = tokenizer.GetNextToken();
		if (next == tok_dot)
		{
			vector<string> pkg = { iden };
			tokenizer.GetNextToken();
			ParsePackageName(pkg);
			type = make_unique<QualifiedIdentifierType>(std::move(pkg));
		}
		else
			type = make_unique<IdentifierType>(iden);
		ParseTemplateArgsForType(static_cast<GeneralIdentifierType*>(type.get()));
	}
	else if (tokenizer.CurTok == tok_script)
	{
		string iden = tokenizer.IdentifierStr;
		tokenizer.GetNextToken();
		type = make_unique<ScriptType>(iden);
	}
	else
	{
		if (basic_types.find(tokenizer.CurTok) != basic_types.end())
			type = make_unique<Type>(tokenizer.CurTok);
		else
			throw CompileError(tokenizer.GetSourcePos(), "Expected an identifier or basic type name");
		tokenizer.GetNextToken();
	}
	return std::move(type);
}

std::unique_ptr<PrototypeAST> ParseFunctionPrototype(ClassAST* cls, bool allow_alias, string* out_link_name,
	bool is_closure = false, bool needs_newline = true, bool needs_name = true);

BD_CORE_API std::unique_ptr<Type> ParseTypeName()
{
	std::unique_ptr<Type> type;
	if (tokenizer.CurTok == tok_functype || tokenizer.CurTok == tok_closure)
	{
		bool is_closure = tokenizer.CurTok == tok_closure;
		tokenizer.GetNextToken();
		type = make_unique<PrototypeType> ( ParseFunctionPrototype(/*cls*/nullptr, /*alias*/false,
			/*out name*/nullptr, /*is_closure*/is_closure, /*newline*/false, /*needs name*/ false) );
	}
	else
		type = ParseBasicType();
	while (tokenizer.CurTok == tok_left_index)
	{
		type->index_level++;
		tokenizer.GetNextToken();//eat [
		CompileExpect(tok_right_index, "Expected  \']\'");
	}
	return std::move(type);
}

std::unique_ptr<Type> ParseType()
{
	//CompileExpect(tok_as, "Expected \'as\'");
	if (tokenizer.CurTok != tok_as)
		return make_unique<Type>(tok_auto);
	tokenizer.GetNextToken(); //eat as
	return ParseTypeName();
}

std::unique_ptr<VariableSingleDefAST> ParseSingleDim()
{
	SourcePos pos = tokenizer.GetSourcePos();
	std::string identifier = tokenizer.IdentifierStr;
	CompileExpect(tok_identifier, "Expected an identifier to define a variable");
	std::unique_ptr<Type> type = ParseType();
	std::unique_ptr<ExprAST> val;
	if (tokenizer.CurTok == tok_assign)
	{
		tokenizer.GetNextToken();
		val = ParseExpressionUnknown();
	}

	return make_unique<VariableSingleDefAST>(identifier, std::move(type), std::move(val), pos);
}

std::unique_ptr<VariableDefAST> ParseDim(bool isglobal = false, bool is_public_global = false,bool* pout_vararg=nullptr,string* pout_vararg_name=nullptr)
{
	std::vector<std::unique_ptr<VariableSingleDefAST>> defs;
	auto pos = tokenizer.GetSourcePos();
	auto process_vararg = [pout_vararg, pout_vararg_name]() {
		CompileAssert(pout_vararg, "\"...\" is only allowed in function parameters");
		tokenizer.GetNextToken();
		*pout_vararg = true;
		if (tokenizer.CurTok == tok_identifier)
		{
			*pout_vararg_name = std::move(tokenizer.IdentifierStr);
			tokenizer.GetNextToken();
		}
		CompileAssert(tokenizer.CurTok==tok_right_bracket, "\"...\" should be the last parameter. Expecting \')\' here.");
	};
	if (tokenizer.CurTok == tok_ellipsis)
	{
		process_vararg();
	}
	else
	{
		auto def = ParseSingleDim();
		defs.push_back(std::move(def));
		for (;;)
		{

			switch (tokenizer.CurTok)
			{
			case tok_comma:
				tokenizer.GetNextToken();
				if (tokenizer.CurTok == tok_ellipsis)
				{
					process_vararg();
					goto done;
				}
				def = ParseSingleDim();
				defs.push_back(std::move(def));
				break;
			case tok_newline:
			case tok_eof:
			case tok_right_bracket:
				goto done;
				break;
			default:
				throw CompileError(tokenizer.GetSourcePos(), "Expected a new line after variable definition");
			}
		}
	}
done:
	if (isglobal)
	{
		for (auto&& v : defs)
		{
			CompileCheckGlobalConflict(v->Pos, v->name);
			std::reference_wrapper<const string> name = v->name;
			cu.dimmap.insert(std::make_pair(name, std::make_pair(v.get(), is_public_global)));
		}
	}

	if (defs.size() == 1)
		return std::move(defs[0]);
	else
		return make_unique<VariableMultiDefAST>(std::move(defs), pos);
}

std::vector<std::unique_ptr<ExprAST>> ParseArguments()
{
	std::vector<std::unique_ptr<ExprAST>> ret;
	if (tokenizer.CurTok == tok_right_bracket)
	{
		tokenizer.GetNextToken();
		return std::move(ret);
	}
	for (;;)
	{
		ret.push_back(ParseExpressionUnknown());
		if (tokenizer.CurTok == tok_right_bracket)
		{
			tokenizer.GetNextToken();
			return ret;
		}
		else if (tokenizer.CurTok == tok_comma)
		{
			tokenizer.GetNextToken();
		}
		else
		{
			throw CompileError("Unexpected Token, expected \')\'");
		}
	}

}

std::unique_ptr<NewExprAST> ParseNew()
{
	SourcePos pos = tokenizer.GetSourcePos();
	tokenizer.GetNextToken();
	auto type = ParseTypeName();
	vector<std::unique_ptr<ExprAST>> expr;
	string method;
	if (tokenizer.CurTok == tok_mul)
	{
		if (tokenizer.GetNextToken() != tok_left_bracket) //new int*6
		{
			type->index_level++;
			expr.push_back(ParseExpressionUnknown());
		}
		else //if tok_left_bracket -> new int*(2,4)
		{
			do {
				tokenizer.GetNextToken(); //eat ( or ,
				type->index_level++;
				expr.push_back(ParseExpressionUnknown());
			} while (tokenizer.CurTok==tok_comma);
			CompileExpect(tok_right_bracket, "Expected )");
		}
	}
	if (type->index_level == 0)
	{
		if (tokenizer.CurTok == tok_colon)
		{
			tokenizer.GetNextToken(); //eat :
			CompileAssert(tokenizer.CurTok == tok_identifier, "Expected an identifier after :");
			method = tokenizer.IdentifierStr;
			tokenizer.GetNextToken();
			if (tokenizer.CurTok == tok_left_bracket)
			{
				tokenizer.GetNextToken(); //eat (
				expr = ParseArguments();
			}
		}
		else if (tokenizer.CurTok == tok_left_bracket) // new Object(...)
		{
			method = "__init__";
			tokenizer.GetNextToken(); // eat (
			expr = ParseArguments();
		}
	}
	return make_unique<NewExprAST>(std::move(type), std::move(expr), method, pos);
}

unique_ptr<ExprAST> ParseIndexOrTemplateInstance(unique_ptr<ExprAST> expr,SourcePos Pos)
{

	tokenizer.GetNextToken();//eat [
	if (tokenizer.CurTok == tok_right_index)
	{
		//if empty []
		tokenizer.GetNextToken();//eat ]
		RunOnTemplateArg(expr.get(),
			[](BasicTypeExprAST* ex) {
			ex->type->index_level++;
		},
			[&expr](IdentifierExprAST* ex) {
			auto type = make_unique<IdentifierType>(ex->Name);
			auto nexpr = make_unique<BasicTypeExprAST>();
			nexpr->type = std::move(type);
			nexpr->type->index_level = 1;
			expr = std::move(nexpr);
		},
			[&expr](MemberExprAST* ex) {
			auto type = make_unique<QualifiedIdentifierType>(ex->ToStringArray());
			auto nexpr = make_unique<BasicTypeExprAST>();
			nexpr->type = std::move(type);
			nexpr->type->index_level = 1;
			expr = std::move(nexpr);
		},
			[](FunctionTemplateInstanceExprAST* ex) {
			assert(0 && "Not implemented yet for template instance array in template argument");
		},
			[](IndexExprAST* ex) {
			assert(0 && "Not implemented yet for template instance array in template argument");
		},
			[](NumberExprAST* ex) {
			throw CompileError("Cannot apply index on number");
		},
			[](StringLiteralAST* ex) {
			throw CompileError("Cannot apply index on string");
		},
			[&expr](ScriptAST* ex) {
			auto type = make_unique<ScriptType>(ex->script);
			auto nexpr = make_unique<BasicTypeExprAST>();
			nexpr->type = std::move(type);
			nexpr->type->index_level = 1;
			expr = std::move(nexpr);
		},
			[]() {
			throw CompileError("Invalid template argument expression type for []");
		}
		);
		return std::move(expr);
	}
	auto firstexpr = CompileExpectNotNull(ParseExpressionUnknown(), "Expected an expression for array index or template");
	if (tokenizer.CurTok == tok_right_index)
	{
		tokenizer.GetNextToken();
		return make_unique<IndexExprAST>(std::move(expr),std::move(firstexpr),Pos);
	}
	vector<unique_ptr<ExprAST>> template_args;
	template_args.emplace_back(std::move(firstexpr));
	while (tokenizer.CurTok == tok_comma)
	{
		tokenizer.GetNextToken();
		template_args.emplace_back(CompileExpectNotNull(ParseExpressionUnknown(), "Expected an expression for template"));
	}
	CompileExpect(tok_right_index, "Expected  \']\'");
	return make_unique<FunctionTemplateInstanceExprAST>(std::move(expr), std::move(template_args), Pos);
}

std::unique_ptr<ExprAST> ParsePrimaryExpression()
{
	std::unique_ptr<ExprAST> firstexpr;

	vector<string> anno;
	while (tokenizer.CurTok == tok_annotation)
	{
		anno.push_back(tokenizer.IdentifierStr);
		tokenizer.GetNextToken();
	}
	auto push_expr = [&anno, &firstexpr](std::unique_ptr<ExprAST>&& st)
	{
		ApplyInternalAnnotations(anno, st.get());
		if (anno.size())
			firstexpr = make_unique<AnnotationStatementAST>(std::move(anno), std::move(st));
		else
			firstexpr = std::move(st);
	};

	switch (tokenizer.CurTok)
	{
	case tok_address_of:
	case tok_pointer_of:
	case tok_typeof:
	{
		auto token = tokenizer.CurTok;
		tokenizer.GetNextToken();
		CompileExpect(tok_left_bracket, "Expected \"(\" after typeof/addressof/pointerof");
		firstexpr = ParseExpressionUnknown();
		CompileExpect(tok_right_bracket, "Expected \')\'");
		// push_expr(make_unique<TypeofExprAST>(std::move(firstexpr), tokenizer.GetSourcePos()));
		push_expr(make_unique<UnaryExprAST>(token, std::move(firstexpr), tokenizer.GetSourcePos()));
		break;
	}
	case tok_left_index:
	{
		auto pos = tokenizer.GetSourcePos();
		tokenizer.GetNextToken();
		vector<unique_ptr<ExprAST>> values;
		while (tokenizer.CurTok != tok_right_index)
		{
			CompileAssert(tokenizer.CurTok != tok_eof, "Unexpected end of file. Unmatched \'[\'.");
			values.push_back(ParseExpressionUnknown());
			if (tokenizer.CurTok != tok_comma)
			{
				CompileAssert(tokenizer.CurTok == tok_right_index, "Expecting \']\' for array initializer");
				break;
			}
			else
			{
				//eat ","
				tokenizer.GetNextToken();
			}
		}
		//eat ]
		tokenizer.GetNextToken();
		push_expr(make_unique<ArrayInitializerExprAST>(std::move(values), pos));
		break;
	}
	case tok_not:
	{
		tokenizer.GetNextToken();
		firstexpr = ParsePrimaryExpression();
		push_expr(make_unique<UnaryExprAST>(tok_not, std::move(firstexpr), tokenizer.GetSourcePos()));
		break;
	}
	case tok_new:
	{
		push_expr(ParseNew());
		break;
	}
	case tok_true:
		push_expr(make_unique<BoolLiteralExprAST>(true));
		tokenizer.GetNextToken();
		break;
	case tok_false:
		push_expr(make_unique<BoolLiteralExprAST>(false));
		tokenizer.GetNextToken();
		break;
	case tok_this:
		push_expr(make_unique<ThisExprAST>());
		tokenizer.GetNextToken();
		break;
	case tok_super:
		push_expr(make_unique<SuperExprAST>());
		tokenizer.GetNextToken();
		break;
	case tok_null:
		push_expr(make_unique<NullExprAST>());
		tokenizer.GetNextToken();
		break;
	case tok_identifier:
		push_expr(make_unique<IdentifierExprAST>(tokenizer.IdentifierStr));
		tokenizer.GetNextToken();
		break;
	case tok_number:
		push_expr(make_unique<NumberExprAST>(tokenizer.NumVal));
		tokenizer.GetNextToken();
		break;
	case tok_string_literal:
		push_expr(make_unique<StringLiteralAST>(tokenizer.IdentifierStr));
		tokenizer.GetNextToken();
		break;
	case tok_left_bracket:
		tokenizer.GetNextToken();
		push_expr(ParseExpressionUnknown());
		CompileExpect(tok_right_bracket, "Expected \')\'");
		break;
	case tok_func:
		tokenizer.GetNextToken(); //eat function
		push_expr(ParseFunction(nullptr));
		return firstexpr; //don't eat the newline token!
		break;
	case tok_eof:
		CompileAssert(anno.empty(), "Annotations cannot be put on the end of the file");
		return nullptr;
		break;
	case tok_script:
		push_expr(make_unique<ScriptAST>(tokenizer.IdentifierStr, false));
		tokenizer.GetNextToken();
		break;
	default:
		if (basic_types.find(tokenizer.CurTok) != basic_types.end())
		{
			CompileAssert(anno.empty(), "Annotations cannot be applied on types");
			firstexpr = make_unique<BasicTypeExprAST>(tokenizer.CurTok,tokenizer.GetSourcePos());
			tokenizer.GetNextToken();
		}
		else
			throw CompileError(tokenizer.GetSourcePos(), "Expected an expression");
	}
	//tokenizer.GetNextToken(); //eat token
	auto parse_tail_token = [&firstexpr]()
	{
		//parse . [] func(), these operators have highest priorities and are parsed from left to right
		auto pos = tokenizer.GetSourcePos();
		std::string member;
		switch (tokenizer.CurTok)
		{
		case tok_left_index:
			firstexpr = ParseIndexOrTemplateInstance(std::move(firstexpr),pos);
			return true;
		case tok_left_bracket:
			tokenizer.GetNextToken();//eat (
			firstexpr = make_unique<CallExprAST>(std::move(firstexpr), ParseArguments());
			return true;
		case tok_dot:
			tokenizer.GetNextToken();//eat .
			member = tokenizer.IdentifierStr;
			CompileExpect(tok_identifier, "Expected an identifier after .");
			firstexpr = make_unique<MemberExprAST>(std::move(firstexpr), member);
			return true;
		default:
			return false;
		}
	};
	while (parse_tail_token()) //check for index/function call
	{
	}
	return firstexpr;
}




std::unordered_map<Token, int> tok_precedence = {
	{ tok_mul,15 },
{ tok_div,15 },
{ tok_mod,15 },
{ tok_add,14 },
{ tok_minus,14 },
{ tok_lt,11 },
{ tok_gt,11 },
{ tok_le,11 },
{ tok_ge,11 },
{ tok_equal,10 },
{ tok_ne,10 },
{ tok_cmp_equal,10 },
{ tok_cmp_ne,10 },
{ tok_and,9 },
{ tok_xor,8 },
{ tok_or,7 },
{ tok_logic_and,6 },
{ tok_logic_or,5 },
{ tok_assign,4 },
};


int GetTokPrecedence()
{
	Token tok = tokenizer.CurTok;
	switch (tokenizer.CurTok)
	{
	case tok_eof:
	case tok_newline:
	case tok_right_bracket:
	case tok_right_index:
	case tok_comma:
	case tok_then:
	case tok_to:
	case tok_till:
		return -1;
		break;
	default:
		auto f = tok_precedence.find(tok);
		if (f != tok_precedence.end())
		{
			return f->second;
		}
		throw CompileError(tokenizer.GetSourcePos(), "Unknown Token");
	}
	return -1;
}

/*
Great code to handle binary operators. Copied from:
https://llvm.org/docs/tutorial/LangImpl02.html
*/
std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
	std::unique_ptr<ExprAST> LHS) {
	// If this is a binop, find its precedence.

	while (true) {
		SourcePos pos = tokenizer.GetSourcePos();
		int TokPrec = GetTokPrecedence();

		// If this is a binop that binds at least as tightly as the current binop,
		// consume it, otherwise we are done.
		if (TokPrec < ExprPrec)
			return LHS;

		// Okay, we know this is a binop.
		Token BinOp = tokenizer.CurTok;
		tokenizer.GetNextToken(); // eat binop

								  // Parse the primary expression after the binary operator.
		auto RHS = CompileExpectNotNull(ParsePrimaryExpression(), "Expected an expression");

		// If BinOp binds less tightly with RHS than the operator after RHS, let
		// the pending operator take RHS as its LHS.
		int NextPrec = GetTokPrecedence();
		if (TokPrec < NextPrec) {
			RHS = CompileExpectNotNull(
				ParseBinOpRHS(TokPrec + 1, std::move(RHS)),
				"Expected an expression");
		}

		// Merge LHS/RHS.
		LHS = make_unique<BinaryExprAST>(BinOp, std::move(LHS),
			std::move(RHS), pos);
	}
}

/*
Parse an expression when we don't know what kind of it is. Maybe an identifier? A function def?
*/
BD_CORE_API std::unique_ptr<ExprAST> ParseExpressionUnknown()
{

	return ParseBinOpRHS(0,
		CompileExpectNotNull(ParsePrimaryExpression(), "Expected an expression"));
}

unique_ptr<TryBlockAST> ParseTry();
std::unique_ptr<ForBlockAST> ParseFor();
std::unique_ptr<WhileBlockAST> ParseWhile();

/*
Parse a basic block, with an optional ending token.
	delimiter - the token that marks the end of the BB other than tok_end
	allow_end - whether the basic block is allowed to end with tok_end
*/
void ParseBasicBlock(ASTBasicBlock& body, Token optional_tok, Token delimiter=tok_error ,bool allow_end=true)
{
	std::unique_ptr<ExprAST> firstexpr;
	SourcePos pos(tokenizer.source_idx,0, 0);
	while (true)
	{
		vector<string> anno;
		while (tokenizer.CurTok == tok_annotation)
		{
			anno.push_back(tokenizer.IdentifierStr);
			tokenizer.GetNextToken();
			while (tokenizer.CurTok == tok_newline)
				tokenizer.GetNextToken();
		}
		auto push_expr = [&anno, &body](std::unique_ptr<StatementAST>&& st)
		{
			ApplyInternalAnnotations(anno, st.get());
			if (anno.size())
				body.body.push_back(make_unique<AnnotationStatementAST>(std::move(anno), std::move(st)));
			else
				body.body.push_back(std::move(st));
		};
		switch (tokenizer.CurTok)
		{
		case tok_continue:
		case tok_break:
			push_expr(make_unique<LoopControlAST>(tokenizer.CurTok));
			tokenizer.GetNextToken();
			break;
		case tok_newline:
			tokenizer.GetNextToken(); //eat newline
			continue;
			break;
		case tok_dim:
			tokenizer.GetNextToken(); //eat dim
			push_expr(std::move(ParseDim()));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after variable definition");
			break;
		case tok_return:
			tokenizer.GetNextToken(); //eat return
			pos = tokenizer.GetSourcePos();
			//assert(current_func_proto && "Current func proto is empty!");
			if (tokenizer.CurTok == tok_newline)
			{
				push_expr(make_unique<ReturnAST>(pos));
			} 
			else 
			{
				push_expr(make_unique<ReturnAST>(ParseExpressionUnknown(), pos));
			}
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line");
			break;
		case tok_func:
		{
			tokenizer.GetNextToken(); //eat function
			auto func = ParseFunction(nullptr);
			CompileAssert(!func->isTemplate(), "Cannot define function template in basic blocks");
			push_expr(std::move(func));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after function definition");
			break;
		}
		case tok_for:
			tokenizer.GetNextToken(); //eat function
			push_expr(std::move(ParseFor()));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after for block");
			break;
		case tok_while:
			tokenizer.GetNextToken(); //eat function
			push_expr(std::move(ParseWhile()));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after while block");
			break;
		case tok_end:
			CompileAssert(allow_end, "\'end\' is not allowed here. A try-block must be followed by a catch-block");
			tokenizer.GetNextToken(); //eat end
			if (optional_tok >= 0 && tokenizer.CurTok == optional_tok) //optional: end function/if/for...
				tokenizer.GetNextToken(); //eat it
			goto done;
			break;
		case tok_else:
			//don't eat else here
			CompileAssert(delimiter == tok_else, "Encountered an unmatched else");
			goto done;
			break;
		case tok_catch:
			//don't eat catch here
			CompileAssert(delimiter == tok_catch, "Encountered an unmatched catch");
			goto done;
			break;
		case tok_eof:
			throw CompileError("Unexpected end of file, missing \"end\"");
			break;
		case tok_if:
			tokenizer.GetNextToken(); //eat if
			push_expr(std::move(ParseIf()));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after if-block");
			break;
		case tok_try:
			tokenizer.GetNextToken(); //eat try
			push_expr(std::move(ParseTry()));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after try-block");
			break;
		case tok_throw:
		{
			auto pos = tokenizer.GetSourcePos();
			tokenizer.GetNextToken();
			push_expr(make_unique<ThrowAST>(ParseExpressionUnknown(),pos));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after throw");
			break;
		}
		default:
			firstexpr = ParseExpressionUnknown();
			BasicBlockCheckUnknownToken({tok_eof,tok_newline });
			CompileAssert(firstexpr != nullptr, "Compiler internal error: firstexpr=null");
			push_expr(std::move(firstexpr));
		}
	}
done:
	return;
}

unique_ptr<TryBlockAST> ParseTry()
{
	SourcePos pos = tokenizer.GetSourcePos();
	CompileAssert(tok_new, "Expecting a newline after try");
	ASTBasicBlock bb;
	ParseBasicBlock(bb, /*optional_tok*/tok_error, /*delimiter*/tok_catch, /*allow_end*/false);
	vector<ASTBasicBlock> catches;
	vector<unique_ptr<VariableSingleDefAST>> catch_vars;
	assert(tokenizer.CurTok == tok_catch);
	while (tokenizer.CurTok == tok_catch)
	{
		tokenizer.GetNextToken(); //eat catch
		catch_vars.emplace_back(ParseSingleDim());
		CompileAssert(catch_vars.back()->val == nullptr, "The exception variable of catch clause must not have initializer");
		CompileExpect(tok_newline, "Expecting a newline after catch clause");
		catches.emplace_back(ASTBasicBlock());
		ParseBasicBlock(catches.back(), /*optional_tok*/tok_try, /*delimiter*/tok_catch,/*allow_end*/true);
	}
	return make_unique<TryBlockAST>(std::move(bb), std::move(catch_vars), std::move(catches), pos);
}

std::unique_ptr<WhileBlockAST> ParseWhile()
{
	SourcePos pos = tokenizer.GetSourcePos();
	auto cond = ParseExpressionUnknown();
	CompileExpect(tok_newline, "Expecting a new line after while condition");
	ASTBasicBlock block;
	ParseBasicBlock(block, tok_while);
	return make_unique<WhileBlockAST>(std::move(cond), std::move(block), pos);
}

std::unique_ptr<ForBlockAST> ParseFor()
{
	SourcePos pos = tokenizer.GetSourcePos();
	bool isdim;
	unique_ptr<StatementAST> init;
	if (tokenizer.CurTok == tok_dim)
	{
		isdim = true;
		tokenizer.GetNextToken();
		init=ParseSingleDim();
	}
	else
	{
		isdim = false;
		init = ParseExpressionUnknown();
	}
	bool including;
	if (tokenizer.CurTok == tok_to)
	{
		including = true;
	}
	else if (tokenizer.CurTok == tok_till)
	{
		including = false;
	}
	else
	{
		throw CompileError("Expect to or till after for");
	}
	tokenizer.GetNextToken();
	auto till = ParseExpressionUnknown();
	CompileExpect(tok_newline, "Expect a new line after for");
	ASTBasicBlock block;
	ParseBasicBlock(block, tok_for);
	return make_unique<ForBlockAST>(std::move(init), std::move(till), including, isdim,std::move(block), pos);
}

std::unique_ptr<IfBlockAST> ParseIf()
{
	SourcePos pos = tokenizer.GetSourcePos();
	std::unique_ptr<ExprAST> cond = ParseExpressionUnknown();
	CompileExpect(tok_then, "Expected \"then\" after the condition of \"if\"");
	CompileExpect(tok_newline, "Expected a newline after \"then\"");
	ASTBasicBlock true_block;
	ASTBasicBlock false_block;
	ParseBasicBlock(true_block, tok_if, tok_else); //allow "else" to mark the end of BB
	if (tokenizer.CurTok == tok_else)
	{
		tokenizer.GetNextToken(); //eat else
		if (tokenizer.CurTok == tok_if) //if it is else-if
		{
			tokenizer.GetNextToken(); //eat if
			false_block.body.push_back(std::move(ParseIf()));
			//if-else don't need to eat the "end" in the end
		}
		else
		{
			CompileExpect(tok_newline, "Expected a newline after \"else\"");
			ParseBasicBlock(false_block, tok_if);
		}
	}
	CompileAssert(tokenizer.CurTok == tok_newline || tokenizer.CurTok == tok_eof, "Expected a new line after \"if\" block");
	return make_unique<IfBlockAST>(std::move(cond), std::move(true_block), std::move(false_block), pos);
}

vector<TemplateParameter> ParseTemplateParameters(bool& is_vararg,string& vararg_name)
{
	vector<TemplateParameter> ret;
	if (tokenizer.CurTok == tok_right_index)
		throw CompileError("The template parameters cannot be empty");
	for (;;)
	{
		if (tokenizer.CurTok == tok_ellipsis)
		{
			tokenizer.GetNextToken();
			is_vararg = true;
			if (tokenizer.CurTok == tok_identifier) //if it is a named vararg
			{
				vararg_name = std::move(tokenizer.IdentifierStr);
				tokenizer.GetNextToken();
			}
			CompileExpect(tok_right_index, "Expecting \"]\" after ..., as it should be the last parameter");
			break;
		}
		else
		{
			CompileAssert(tokenizer.CurTok == tok_identifier, "Expected an identifier");
			string identifier = tokenizer.IdentifierStr;
			tokenizer.GetNextToken();
			if (tokenizer.CurTok == tok_as)
			{
				unique_ptr<Type> ty = ParseType();
				if (ty->index_level > 0)
					throw CompileError("Arrays are not supported in template parameters");
				ret.push_back(TemplateParameter(std::move(ty), identifier));
			}
			else
			{
				ret.push_back(TemplateParameter(nullptr, identifier));
			}
		}
		if (tokenizer.CurTok == tok_right_index)
		{
			tokenizer.GetNextToken();
			break;
		}
		else if (tokenizer.CurTok == tok_comma)
		{
			tokenizer.GetNextToken();
		}
		else
		{
			throw CompileError("Unexpected token in template parameters");
		}
	}
	return ret;
}

std::unique_ptr<FunctionAST> ParseFunction(ClassAST* cls)
{
	auto pos = tokenizer.GetSourcePos();
	unique_ptr<TemplateParameters<FunctionAST>> template_param;
	std::string name;
	int view_pos = 0; //the string view position within the parent template,0 if is top level template
	bool is_template = false;
	if (tokenizer.CurTok == tok_identifier)
	{
		name = tokenizer.IdentifierStr;
		tokenizer.GetNextToken();
	}
	else
	{
		//if it is an unnamed function
		std::stringstream buf;
		buf << ".unnamed_function" << unnamed_func_cnt;
		unnamed_func_cnt++;
		name = buf.str();
	}
	if (tokenizer.CurTok == tok_left_index)
	{
		is_template = true;
		if (!tokenizer.is_recording)
		{
			tokenizer.StartRecording(string("function ") + name + " [");
		}
		else
		{
			view_pos=tokenizer.GetTemplateSourcePosition()-1;
		}
		tokenizer.GetNextToken();
		template_param = make_unique<TemplateParameters<FunctionAST>>();
		template_param->params=std::move(ParseTemplateParameters(template_param->is_vararg,template_param->vararg_name));
	}
	CompileExpect(tok_left_bracket, "Expected \'(\'");
	std::unique_ptr<VariableDefAST> args;
	bool is_vararg = false;
	string vararg_name;
	if (tokenizer.CurTok != tok_right_bracket)
		args = ParseDim(/*is_global*/false, /*is_public_global*/false, &is_vararg, &vararg_name);
	if (is_vararg)
	{
		CompileAssert(is_template || (cls && cls->isTemplate()), "Vararg in function parameters can only be used within a function or class template");
	}
	CompileExpect(tok_right_bracket, "Expected \')\'");
	auto rettype = ParseType();
	bool is_return_void = false;
	if (rettype->type == tok_auto)
	{
		is_return_void = true;
		rettype->type = tok_void;
	}
	auto funcproto = make_unique<PrototypeAST>(name, std::move(args), std::move(rettype), cls, tokenizer.GetSourcePos(),/*is_closure*/false);
	auto old_current_func_proto = current_func_proto;
	current_func_proto = funcproto.get();

	ASTBasicBlock body;

	if (tokenizer.CurTok == tok_into)
	{
		//one-line function
		tokenizer.GetNextToken();
		unique_ptr<ExprAST> expr = ParseExpressionUnknown();
		auto pos = expr->Pos;
		unique_ptr<StatementAST> stmt;
		if (!is_return_void)
			stmt = make_unique<ReturnAST>(std::move(expr), pos);
		else
			stmt = std::move(expr);
		body.body.emplace_back(std::move(stmt));
	}
	else
	{
		//parse function body
		ParseBasicBlock(body, tok_func);
	}
	current_func_proto = old_current_func_proto;
	if (is_template)
	{
		if (!view_pos)
			template_param->source.set(tokenizer.StopRecording());
		else
		{
			auto curlen = tokenizer.GetTemplateSourcePosition();
			assert(curlen > view_pos);
			template_param->source.set(string("function ") + name + " [", view_pos, curlen - 1 - view_pos);
		}
	}

	return make_unique<FunctionAST>(std::move(funcproto), std::move(body), std::move(template_param), is_vararg,std::move(vararg_name), pos);
}

std::unique_ptr<PrototypeAST> ParseFunctionPrototype(ClassAST* cls, bool allow_alias, string* out_link_name , bool is_closure , bool needs_newline ,bool needs_name)
{
	auto pos = tokenizer.GetSourcePos();
	std::string name;
	if (needs_name)
	{
		 name = tokenizer.IdentifierStr;
		CompileExpect(tok_identifier, "Expected an identifier");
	}
	string& link_name=*out_link_name;
	if (tokenizer.CurTok == tok_alias)
	{
		CompileAssert(allow_alias, "Alias is not allowed here, in function types");
		if (tokenizer.GetNextToken() != tok_string_literal)
		{
			throw CompileError("Expected a string literal after alias");
		}
		link_name = tokenizer.IdentifierStr;
		tokenizer.GetNextToken();
	}
	CompileExpect(tok_left_bracket, "Expected \'(\'");
	std::unique_ptr<VariableDefAST> args;
	if (tokenizer.CurTok != tok_right_bracket)
		args = ParseDim();
	CompileExpect(tok_right_bracket, "Expected \')\'");
	auto rettype = ParseType();
	if (needs_newline)
		CompileExpect({ tok_newline, tok_eof }, "Expected a new line after function declaration");
	if (rettype->type == tok_auto)
		rettype->type = tok_void;
	return make_unique<PrototypeAST>(name, std::move(args), std::move(rettype), cls, pos,is_closure);
}

std::unique_ptr<FunctionAST> ParseDeclareFunction(ClassAST* cls)
{
	string link_name;
	auto funcproto = ParseFunctionPrototype(cls, true, &link_name);
	return make_unique<FunctionAST>(std::move(funcproto), link_name, funcproto->pos);
}

//is_field: true for FieldDef, false for MemberFunctionDef
//index: the index within fields/funcdef
static unordered_map<string, std::function<void(ClassAST* cls, bool is_field, int index)>> interal_class_annontation_map = {
	{"virtual", [](ClassAST* cls, bool is_field, int index) {
		CompileAssert(!is_field,"The \'virtual\' annotation can only be applied on member functions");
		CompileAssert(!cls->is_struct, "The \'virtual\' annotation can only be applied on class functions");
		CompileAssert(!cls->isTemplate(), "The \'virtual\' annotation cannot be applied on class templates");
		cls->needs_rtti = true;
		//it is set to VIRT_UNRESOLVED if is marked virtual but unresolved before Phase0
		cls->funcs[index].virtual_idx = MemberFunctionDef::VIRT_UNRESOLVED;
	}}
};

BD_CORE_API bool ParseClassBody(ClassAST* ret)
{
	std::vector<FieldDef>& fields = ret->fields;
	std::vector<MemberFunctionDef>& funcs = ret->funcs;
	unordered_map<reference_wrapper<const string>, int>& fieldmap = ret->fieldmap;
	unordered_map<reference_wrapper<const string>, int>& funcmap = ret->funcmap;
	auto classname_checker = [&fieldmap, &funcmap, &fields, &funcs](SourcePos pos, const string& name)
	{
		auto prv1 = fieldmap.find(name);
		if (prv1 != fieldmap.end())
		{
			throw CompileError(pos, string("The name ") + name + " is already used in " + fields[prv1->second].decl->Pos.ToString());
		}
		auto prv2 = funcmap.find(name);
		if (prv2 != funcmap.end())
		{
			throw CompileError(pos, string("The name ") + name + " is already used in " + funcs[prv2->second].decl->Pos.ToString());
		}
	};
	AccessModifier access;
	while (tokenizer.CurTok == tok_newline) tokenizer.GetNextToken();
	std::vector<string> anno;
	while (tokenizer.CurTok == tok_annotation)
	{
		anno.emplace_back(std::move(tokenizer.IdentifierStr));
		tokenizer.GetNextToken();
		while (tokenizer.CurTok == tok_newline) tokenizer.GetNextToken();
	}
	auto apply_annotations = [&anno, ret](bool is_field, int index)
	{
		for (auto& ano : anno)
		{
			auto itr = interal_class_annontation_map.find(ano);
			CompileAssert(itr != interal_class_annontation_map.end(), string("Unrecognized annotation: ") 
				+ ano + " ,note that class members only supports internal annotations. Python annotations are not supported");
			itr->second(ret, is_field, index);
		}
	};
	switch (tokenizer.CurTok)
	{
	case tok_dim:
		throw CompileError("You should use public/private to define a member variable in a class");
		break;
	case tok_private:
	case tok_public:
		access = tokenizer.CurTok == tok_private ? AccessModifier::access_private : AccessModifier::access_public;
		tokenizer.GetNextToken(); //eat access modifier
		if (tokenizer.CurTok == tok_identifier)
		{
			auto var = ParseDim();
			SourcePos pos = var->Pos;
			var->move(std::move(var), [pos, &classname_checker, &fields, access, &fieldmap](unique_ptr<VariableSingleDefAST>&& def) {
				classname_checker(pos, def->name);
				fields.push_back(FieldDef(access, std::move(def), fields.size()));
				fieldmap.insert(std::make_pair(reference_wrapper<const string>(fields.back().decl->name), fields.size() - 1));
			});
			apply_annotations(true, fields.size() - 1);
			CompileExpect(tok_newline, "Expected a new line after variable definition");

		}
		else if (tokenizer.CurTok == tok_func)
		{
			tokenizer.GetNextToken(); //eat function
			funcs.push_back(MemberFunctionDef(access, ParseFunction(ret)));
			classname_checker(funcs.back().decl->Pos, funcs.back().decl->GetName());
			funcmap.insert(std::make_pair(reference_wrapper<const string>(funcs.back().decl->GetName()),
				funcs.size() - 1));
			CompileExpect(tok_newline, "Expected a new line after function definition");
			apply_annotations(false, funcs.size() - 1);
		}
		else
		{
			throw CompileError("After public/private, only accept function/variable definitions");
		}
		break;
	case tok_func:
		tokenizer.GetNextToken(); //eat function
		funcs.push_back(MemberFunctionDef(access_private, ParseFunction(ret)));
		classname_checker(funcs.back().decl->Pos, funcs.back().decl->GetName());
		funcmap.insert(std::make_pair(reference_wrapper<const string>(funcs.back().decl->GetName()),
			funcs.size() - 1));
		CompileExpect({ tok_newline }, "Expected a new line after function definition");
		apply_annotations(false, funcs.size() - 1);
		break;
	case tok_declare:
		tokenizer.GetNextToken(); //eat declare
		CompileExpect(tok_func, "Expected a function after declare");
		funcs.push_back(MemberFunctionDef(access_private, ParseDeclareFunction(ret)));
		classname_checker(funcs.back().decl->Pos, funcs.back().decl->GetName());
		funcmap.insert(std::make_pair(reference_wrapper<const string>(funcs.back().decl->GetName()),
			funcs.size() - 1));
		apply_annotations(false, funcs.size() - 1);
		break;
	default :
		return false;
	}
	return true;
}

void ParseClassInPlace(ClassAST* ret, bool is_struct)
{
	auto pos = tokenizer.GetSourcePos();
	std::string name = tokenizer.IdentifierStr;
	CompileExpect(tok_identifier, "Expected an identifier for class name");
	//std::unique_ptr<ClassAST> ret = make_unique<ClassAST>(name, pos);
	ret->name = name;
	ret->Pos = pos;
	ret->is_struct = is_struct;
	int view_pos = 0;
	bool is_template = false;
	if (tokenizer.CurTok == tok_left_index)
	{
		is_template = true;
		if (!tokenizer.is_recording)
		{
			tokenizer.StartRecording(string(is_struct? "struct ":"class ") + name + " [");
		}
		else
		{
			view_pos = tokenizer.GetTemplateSourcePosition()-1;
		}
		tokenizer.GetNextToken();
		ret->template_param = make_unique<TemplateParameters<ClassAST>>();
		ret->template_param->params = std::move(ParseTemplateParameters(ret->template_param->is_vararg, ret->template_param->vararg_name));
	}
	// class inherit
	
	if (tokenizer.CurTok == tok_colon) {
		CompileAssert(!is_struct, "structs do not support inherience");
		tokenizer.GetNextToken(); // eat colon
		// CompileExpect(tok_class, "Expected a class as parent"); // eat 'class'
		// tokenizer.GetNextToken(); 
		CompileAssert(tokenizer.CurTok == tok_identifier, "Expected a class name");
		// ret->parent = make_unique<VariableSingleDefAST>(std::move(ParseBasicType()), pos);
		ret->parent_type = ParseBasicType();
	}
	
	CompileExpect(tok_newline, "Expected an newline after class name");

	while (true)
	{
		if (ParseClassBody(ret)) //if a member is recoginzed, continue to find next one
			continue;
		if (tokenizer.CurTok == tok_end)
		{
			tokenizer.GetNextToken(); //eat end
			if (is_struct)
			{
				if (tokenizer.CurTok == tok_class)
					throw CompileError("Expecting \"struct\" after \"end\", no \"class\" allowed here");
				if (tokenizer.CurTok == tok_struct)
					tokenizer.GetNextToken();
			}
			else
			{
				if (tokenizer.CurTok == tok_class)
					tokenizer.GetNextToken();
				if (tokenizer.CurTok == tok_struct)
					throw CompileError("Expecting \"class\" after \"end\", no \"struct\" allowed here");
			}
			goto done;
		}
		else if(tokenizer.CurTok == tok_eof)
			throw CompileError("Unexpected end of file, missing \"end\"");
		else
		{
			throw CompileError("Expected member declarations");
		}
	}
done:
	if (is_template)
	{
		if (!view_pos)
			ret->template_param->source.set(tokenizer.StopRecording());
		else
		{
			auto curlen = tokenizer.GetTemplateSourcePosition();
			assert(curlen > view_pos);
			ret->template_param->source.set(string(is_struct ? "struct " : "class ") + name + " [", view_pos, curlen - 1 - view_pos);
		}
	}
	//return std::move(ret);
}
std::unique_ptr<ClassAST> ParseClass(bool is_struct)
{
	std::unique_ptr<ClassAST> ret = make_unique<ClassAST>(string(), SourcePos(0,0,0));
	ParseClassInPlace(ret.get(), is_struct);
	return std::move(ret);
}
void ParsePackageName(vector<string>& ret)
{
	CompileAssert(tokenizer.CurTok == tok_identifier, "Unexpected token in the package name");
	ret.push_back(tokenizer.IdentifierStr);
	tokenizer.GetNextToken();
	while (tokenizer.CurTok==tok_dot)
	{
		tokenizer.GetNextToken();
		CompileAssert(tokenizer.CurTok == tok_identifier, "Unexpected token in the package name");
		ret.push_back(tokenizer.IdentifierStr);
		tokenizer.GetNextToken();
	}
}

vector<string> ParsePackageName()
{
	vector<string> ret;
	ParsePackageName(ret);
	return ret;;
}

namespace Birdee
{
	extern bool IsIntrinsicModule(const string& name);
}

void ParsePackage()
{
	if (tokenizer.CurTok == tok_package)
	{
		tokenizer.GetNextToken();
		vector<string> package = ParsePackageName();
		CompileExpect({ tok_newline,tok_eof }, "Expected a new line after package name");
		cu.symbol_prefix = "";
		for (auto& str : package)
		{
			cu.symbol_prefix += str;
			cu.symbol_prefix += '.';
		}
	}
	size_t found;
	found = cu.filename.find_last_of('.');
	if (found == string::npos)
	{
		cu.symbol_prefix += cu.filename;
	}
	else
	{
		cu.symbol_prefix += cu.filename.substr(0, found);
	}
	cu.is_intrinsic = IsIntrinsicModule(cu.symbol_prefix);
	cu.symbol_prefix += '.';

}

ImportTree * Birdee::ImportTree::FindName(const string & name) const
{
	auto itr = map.find(name);
	if (itr != map.end())
		return itr->second.get();
	return nullptr;
}

ImportTree* Birdee::ImportTree::Contains(const string& package, int sz, int level)
{
	if (level >= sz)
	{
		if (map.empty())
			return this;
		else
			return nullptr;
	}
	auto idx = package.find('.', level);
	if (idx == string::npos)
		idx = sz;
	auto ptr = FindName(package.substr(level,idx-level));
	if (ptr)
		return ptr->Contains(package, sz, idx +1);
	return nullptr;
}

ImportTree* Birdee::ImportTree::Contains(const vector<string>& package,int level)
{
	if (level == package.size())
	{
		if(map.empty())
			return this;
		return nullptr;
	}		
	auto ptr = FindName(package[level]);
	if (ptr)
		return ptr->Contains(package, level + 1);
	return nullptr;
}

ImportTree* Birdee::ImportTree::Insert(const vector<string>& package, int level)
{
	if (level == package.size())
	{
		return this;
	}
	auto ptr = FindName(package[level]);
	if (ptr)
		return ptr->Insert(package, level + 1);
	else
	{
		auto nptr = std::make_unique<Birdee::ImportTree>();
		auto ret = nptr->Insert(package, level + 1);
		map[package[level]] = std::move(nptr);
		return ret;
	}
}

BD_CORE_API string GetModuleNameByArray(const vector<string>& package, const char* delimiter = ".")
{
	string ret=package[0];
	for (int i = 1; i < package.size(); i++)
	{
		ret += delimiter;
		ret += package[i];
	}
	return ret;
}

BD_CORE_API ImportedModule* DoImportPackage(const vector<string>& package)
{
	auto prv = cu.imported_packages.Contains(package);
	if(prv)
		return prv->mod.get();
	ImportTree* node=cu.imported_packages.Insert(package);
	string name = GetModuleNameByArray(package);
	cu.imported_module_names.push_back(name);
	node->mod = make_unique<ImportedModule>();
	node->mod->Init(package, name);
	return node->mod.get();
}


using strref = std::reference_wrapper<const string>;
void InsertName(unordered_map<strref, ClassAST*>& imported_classmap,const string& name,ClassAST* ptr)
{
	auto itr2 = imported_classmap.find(name);
	WarnAssert(itr2 == imported_classmap.end(),
		string("The imported class ") + ptr->GetUniqueName()
		+ " has overwritten the previously imported class " + itr2->second->GetUniqueName());
	if(itr2!= imported_classmap.end()) imported_classmap.erase(itr2);
	imported_classmap[strref(name)] = ptr;
}
void InsertName(unordered_map<strref, VariableSingleDefAST*>& imported_dimmap, const string& name, VariableSingleDefAST* ptr)
{
	auto itr2 = imported_dimmap.find(name);
	WarnAssert(itr2 == imported_dimmap.end(),
		string("The imported variable ") + ptr->name
		+ " has overwritten the previously imported variable.");
	if (itr2 != imported_dimmap.end()) imported_dimmap.erase(itr2);
	imported_dimmap.insert(std::make_pair(strref(name), ptr));
}


void InsertName(unordered_map<strref, FunctionAST*>& imported_funcmap, const string& name, FunctionAST* ptr)
{
	auto itr2 = imported_funcmap.find(name);
	WarnAssert(itr2 == imported_funcmap.end(),
		string("The imported function ") + ptr->GetName()
		+ " has overwritten the previously imported function.");
	if (itr2 != imported_funcmap.end()) imported_funcmap.erase(itr2);
	imported_funcmap.insert(std::make_pair(strref(name), ptr));
}

void InsertName(unordered_map<strref, PrototypeAST*>& imported_funcmap, const string& name, PrototypeAST* ptr)
{
	auto itr2 = imported_funcmap.find(name);
	WarnAssert(itr2 == imported_funcmap.end(),
		string("The imported function type ") + ptr->GetName()
		+ " has overwritten the previously imported function type.");
	if (itr2 != imported_funcmap.end()) imported_funcmap.erase(itr2);
	imported_funcmap.insert(std::make_pair(strref(name), ptr));
}
void DoImportPackageAll(const vector<string>& package)
{
	auto mod = DoImportPackage(package);
	for (auto& itr : mod->classmap)
	{
		if(itr.second.second)
			InsertName(cu.imported_classmap, itr.first, itr.second.first.get());
	}
	for (auto& itr : mod->dimmap)
	{
		if (itr.second.second)
			InsertName(cu.imported_dimmap, itr.first, itr.second.first.get());
	}
	for (auto& itr : mod->funcmap)
	{
		if (itr.second.second)
			InsertName(cu.imported_funcmap, itr.first, itr.second.first.get());
	}
	for (auto& itr : mod->functypemap)
	{
		if (itr.second.second)
			InsertName(cu.imported_functypemap, itr.first, itr.second.first.get());
	}
}

template<typename T,typename NodeT>
bool FindAndInsertName(NodeT& node,T& namemap, const string& name)
{
	auto dim = namemap.find(name);
	if (dim != namemap.end() && dim->second.second)
	{
		InsertName(node,dim->first, dim->second.first.get());
		return true;
	}
	return false;
}

BD_CORE_API void DoImportName(const vector<string>& package, const string& name)
{
	auto mod = DoImportPackage(package);
	if (FindAndInsertName(cu.imported_dimmap, mod->dimmap, name)) return;
	if (FindAndInsertName(cu.imported_funcmap, mod->funcmap, name)) return;
	if (FindAndInsertName(cu.imported_classmap, mod->classmap, name)) return;
	if (FindAndInsertName(cu.imported_functypemap, mod->functypemap, name)) return;
	throw CompileError("Cannot find name " + name + " from module " + GetModuleNameByArray(package));
}


static vector<vector<string>> auto_import_packages = { {"birdee"} };

BD_CORE_API void AddAutoImport()
{
	for(auto& imp:auto_import_packages)
		DoImportPackageAll(imp);
}


void DoImportNameInImportedModule(ImportedModule* to_mod,const vector<string>& package, const string& name)
{
	auto mod = DoImportPackage(package);
	if (FindAndInsertName(to_mod->imported_dimmap, mod->dimmap, name)) return;
	if (FindAndInsertName(to_mod->imported_funcmap, mod->funcmap, name)) return;
	if (FindAndInsertName(to_mod->imported_classmap, mod->classmap, name)) return;
	if (FindAndInsertName(to_mod->imported_functypemap, mod->functypemap, name)) return;
	throw CompileError("Cannot find name " + name + "from module " + GetModuleNameByArray(package));
}

void DoImportPackageAllInImportedModule(ImportedModule* to_mod, const vector<string>& package)
{
	auto mod = DoImportPackage(package);
	for (auto& itr : mod->classmap)
	{
		if (itr.second.second)
			InsertName(to_mod->imported_classmap, itr.first, itr.second.first.get());
	}
	for (auto& itr : mod->dimmap)
	{
		if (itr.second.second)
			InsertName(to_mod->imported_dimmap, itr.first, itr.second.first.get());
	}
	for (auto& itr : mod->funcmap)
	{
		if (itr.second.second)
			InsertName(to_mod->imported_funcmap, itr.first, itr.second.first.get());
	}
	for (auto& itr : mod->functypemap)
	{
		if (itr.second.second)
			InsertName(to_mod->imported_functypemap, itr.first, itr.second.first.get());
	}
}

void ImportedModule::HandleImport()
{
	if (imported_classmap.size() == 0)
	{
		for (auto& imp : auto_import_packages)
			DoImportPackageAllInImportedModule(this, imp);
	}
	if (user_imports.size())
	{
		for (auto& imp : user_imports)
		{
			if (imp.back() == "*")
			{
				imp.pop_back();
				DoImportPackageAllInImportedModule(this, imp);
			}
			else if (imp.back().size() > 0 && imp.back()[0] == ':')
			{
				string name = imp.back().substr(1);
				imp.pop_back();
				DoImportNameInImportedModule(this, imp, name);
			}
			else
			{
				DoImportPackage(imp);
			}
		}
		user_imports.clear();
	}
}

void ParseImports(bool need_do_import)
{
	for (;;)
	{
		if (tokenizer.CurTok == tok_newline)
		{
			tokenizer.GetNextToken();
			continue;
		}
		else if (tokenizer.CurTok == tok_import)
		{
			tokenizer.GetNextToken();
			vector<string> package = ParsePackageName();
			if (tokenizer.CurTok == tok_newline || tokenizer.CurTok == tok_eof)
			{
				if(need_do_import)
					DoImportPackage(package);
				cu.imports.push_back(std::move(package));
				tokenizer.GetNextToken();
			}
			else if (tokenizer.CurTok == tok_colon)
			{
				tokenizer.GetNextToken();
				if (tokenizer.CurTok == tok_identifier)
				{
					string name = tokenizer.IdentifierStr;
					tokenizer.GetNextToken();
					CompileExpect({ tok_newline,tok_eof }, "Expected a new line after import");
					if(need_do_import)
						DoImportName(package, name);
					package.push_back(string(":") + name);
					cu.imports.push_back(std::move(package));
				}
				else if (tokenizer.CurTok == tok_mul)
				{
					tokenizer.GetNextToken();
					CompileExpect({ tok_newline,tok_eof }, "Expected a new line after import");
					if(need_do_import)
						DoImportPackageAll(package);
					package.push_back("*");
					cu.imports.push_back(std::move(package));
				}
			}
		}
		else
			break;
	}
}

BD_CORE_API unique_ptr<StatementAST> ParseStatement(bool is_top_level)
{
	unique_ptr<StatementAST> ret;
	switch (tokenizer.CurTok)
	{
	case tok_newline:
		tokenizer.GetNextToken(); //eat newline
		break;
	case tok_for:
		tokenizer.GetNextToken(); //eat for
		ret=ParseFor();
		CompileExpect({ tok_newline,tok_eof }, "Expected a new line after for block");
		break;
	case tok_while:
		tokenizer.GetNextToken(); //eat while
		ret = ParseWhile();
		CompileExpect({ tok_newline,tok_eof }, "Expected a new line after while block");
		break;
	case tok_if:
		tokenizer.GetNextToken(); //eat if
		ret = ParseIf();
		CompileExpect({ tok_newline,tok_eof }, "Expected a new line after if-block");
		break;
	case tok_try:
		tokenizer.GetNextToken(); //eat try
		ret = ParseTry();
		CompileExpect({ tok_newline,tok_eof }, "Expected a new line after try-block");
		break;
	case tok_throw:
	{
		auto pos = tokenizer.GetSourcePos();
		tokenizer.GetNextToken();
		ret = make_unique<ThrowAST>(ParseExpressionUnknown(), pos);
		CompileExpect({ tok_newline,tok_eof }, "Expected a new line after throw");
		break;
	}
	case tok_declare:
	{
		tokenizer.GetNextToken(); //eat declare
		CompileExpect(tok_func, "Expected a function after declare");
		CompileAssert(is_top_level, "Can only parse function declaration in top-level");
		std::unique_ptr<FunctionAST> funcast = ParseDeclareFunction(nullptr);
		std::reference_wrapper<const string> funcname = funcast->GetName();
		CompileCheckGlobalConflict(funcast->Pos, funcname);
		cu.funcmap.insert(std::make_pair(funcname, std::make_pair(funcast.get(), true)));
		ret = std::move(funcast);
		break;
	}
	case tok_return:
	{
		tokenizer.GetNextToken(); //eat return
		auto pos = tokenizer.GetSourcePos();
		if (tokenizer.CurTok == tok_newline || tokenizer.CurTok == tok_eof)
		{
			ret = make_unique<ReturnAST>(pos);
		}
		else
		{
			ret = make_unique<ReturnAST>(ParseExpressionUnknown(), pos);
		}
		CompileExpect({ tok_newline,tok_eof }, "Expected a new line");
		break;
	}
	default:
		ret = ParseExpressionUnknown();
		BasicBlockCheckUnknownToken({ tok_eof,tok_newline });
		CompileAssert(ret != nullptr, "Compiler internal error: firstexpr=null");
	}
	return std::move(ret);
}

BD_CORE_API void ParseTopLevelImportsOnly()
{
	tokenizer.GetNextToken();
	while (tokenizer.CurTok == tok_newline)
		tokenizer.GetNextToken();
	ParsePackage();
	ParseImports(/*need_do_import*/false);
}

BD_CORE_API int ParseTopLevel(bool autoimport)
{
	std::vector<std::unique_ptr<StatementAST>>& out = cu.toplevel;
	std::unique_ptr<ExprAST> firstexpr;
	tokenizer.GetNextToken();
	while (tokenizer.CurTok == tok_newline)
		tokenizer.GetNextToken();
	if (autoimport)
		ParsePackage();
	if(autoimport &&!cu.is_corelib)
		AddAutoImport();
	ParseImports(/*need_do_import*/true);
	
	while (tokenizer.CurTok != tok_eof && tokenizer.CurTok != tok_error)
	{
		vector<string> anno;
		while (tokenizer.CurTok == tok_annotation)
		{
			anno.push_back(tokenizer.IdentifierStr);
			tokenizer.GetNextToken();
			while(tokenizer.CurTok==tok_newline) tokenizer.GetNextToken();
		}
		auto is_public = [&anno]()
		{
			bool ret = true;
			for (auto itr = anno.begin(); itr != anno.end();)
			{
				if (*itr == "private")
				{
					itr = anno.erase(itr);
					ret = false;
				}
				else
				{
					++itr;
				}
			}
			return ret;
		};
		auto push_expr = [&anno, &out](std::unique_ptr<StatementAST>&& st)
		{
			ApplyInternalAnnotations(anno, st.get(), true);
			if (anno.size())
				out.push_back(make_unique<AnnotationStatementAST>(std::move(anno), std::move(st)));
			else
				out.push_back(std::move(st));
		};
		switch (tokenizer.CurTok)
		{
		case tok_newline:
			CompileAssert(anno.size() == 0, "No empty line allowed after annotations");
			tokenizer.GetNextToken(); //eat newline
			continue;
			break;
		case tok_dim:
			tokenizer.GetNextToken(); //eat dim
			push_expr(ParseDim(true,is_public()));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after variable definition");
			break;
		case tok_eof:
			CompileAssert(anno.empty(), "Annotations cannot be put on the end of the file");
			return 0;
			break;
		case tok_for:
			tokenizer.GetNextToken(); //eat for
			push_expr(ParseFor());
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after for block");
			break;
		case tok_while:
			tokenizer.GetNextToken(); //eat while
			push_expr(ParseWhile());
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after while block");
			break;
		case tok_func:
		{
			tokenizer.GetNextToken(); //eat function
			std::unique_ptr<FunctionAST> funcast = ParseFunction(nullptr);
			std::reference_wrapper<const string> funcname = funcast->GetName();
			CompileCheckGlobalConflict(funcast->Pos, funcname);
			cu.funcmap.insert(std::make_pair(funcname, std::make_pair(funcast.get(),is_public())));
			push_expr(std::move(funcast));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after function definition");
			break;
		}
		case tok_functype:
		case tok_closure:
		{
			CompileAssert(anno.size() == 0, "No function type allowed after annotations");
			bool is_closure = tokenizer.CurTok == tok_closure;
			tokenizer.GetNextToken(); //eat function
			auto funcast = ParseFunctionPrototype(nullptr, false, nullptr, is_closure);
			std::reference_wrapper<const string> funcname = funcast->GetName();
			CompileCheckGlobalConflict(funcast->pos, funcname);
			cu.functypemap.insert(std::make_pair(funcname, std::make_pair(std::move(funcast),is_public())));
			break;
		}
		case tok_declare:
		{
			tokenizer.GetNextToken(); //eat declare
			CompileExpect(tok_func, "Expected a function after declare");
			std::unique_ptr<FunctionAST> funcast = ParseDeclareFunction(nullptr);
			std::reference_wrapper<const string> funcname = funcast->GetName();
			CompileCheckGlobalConflict(funcast->Pos, funcname);
			cu.funcmap.insert(std::make_pair(funcname, std::make_pair(funcast.get(), is_public())));
			push_expr(std::move(funcast));
			break;
		}
		case tok_if:
			tokenizer.GetNextToken(); //eat if
			push_expr(std::move(ParseIf()));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after if-block");
			break;
		case tok_try:
			tokenizer.GetNextToken(); //eat try
			push_expr(std::move(ParseTry()));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after try-block");
			break;
		case tok_throw:
		{
			auto pos = tokenizer.GetSourcePos();
			tokenizer.GetNextToken();
			push_expr(make_unique<ThrowAST>(ParseExpressionUnknown(), pos));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after throw");
			break;
		}
		case tok_class:
		case tok_struct:
		{
			bool is_struct = tokenizer.CurTok == tok_struct;
			tokenizer.GetNextToken(); //eat class
			auto classdef = ParseClass(is_struct);
			std::reference_wrapper<const string> clscname = classdef->name;
			CompileCheckGlobalConflict(classdef->Pos, clscname);
			cu.classmap.insert(std::make_pair(clscname, std::make_pair(classdef.get(),is_public())));
			push_expr(std::move(classdef));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after class definition");
			break;
		}
		case tok_script:
			push_expr(make_unique<ScriptAST>(tokenizer.IdentifierStr,true));
			tokenizer.GetNextToken();
			break;
		default:
			firstexpr = ParseExpressionUnknown();
			BasicBlockCheckUnknownToken({ tok_eof,tok_newline });
			//if (!firstexpr)
			//	break;
			CompileAssert(firstexpr != nullptr, "Compiler internal error: firstexpr=null");
			push_expr(std::move(firstexpr));
		}
		tokenizer.SetCanEOF();
	}

	return 0;
}

Birdee::AnnotationStatementAST::AnnotationStatementAST(vector<string>&& anno, unique_ptr<StatementAST>&& impl)
	:anno(std::move(anno)), impl(std::move(impl))
{
	is_expr = instance_of<ExprAST>(this->impl.get());
}