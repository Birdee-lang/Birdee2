
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
#include "AST.h"
#include "CompileError.h"
#include <assert.h>
#include <unordered_set>
#include <iostream>
using namespace Birdee;

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//




extern Tokenizer tokenizer;


namespace Birdee
{
	SourcePos GetCurrentSourcePos()
	{
		return SourcePos{ tokenizer.GetLine(),tokenizer.GetPos() };
	}
	string GetTokenString(Token tok)
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
#define WarnAssert(_b,_msg) (_b?0:DoWarn(_msg));

inline void CompileExpect(Token expected_tok, const std::string& msg)
{
	if (tokenizer.CurTok != expected_tok)
	{
		throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), msg);
	}

	tokenizer.GetNextToken();
}


inline void CompileAssert(bool a, const std::string& msg)
{
	if (!a)
	{
		throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), msg);
	}
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
	throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), msg);
}




inline void CompileCheckGlobalConflict(SourcePos pos, const std::string& name)
{
	auto prv_cls = cu.classmap.find(name);
	if (prv_cls != cu.classmap.end())
	{
		throw CompileError(pos.line, pos.pos, string("The global name ") + name + " is already defined in " + prv_cls->second.get().Pos.ToString());
	}
	auto prv_func = cu.funcmap.find(name);
	if (prv_func != cu.funcmap.end())
	{
		throw CompileError(pos.line, pos.pos, string("The global name ") + name + " is already defined in " + prv_func->second.get().Pos.ToString());
	}
	auto prv_dim = cu.dimmap.find(name);
	if (prv_dim != cu.dimmap.end())
	{
		throw CompileError(pos.line, pos.pos, string("The global name ") + name + " is already defined in " + prv_func->second.get().Pos.ToString());
	}
}
/////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<ExprAST> ParseExpressionUnknown();
std::unique_ptr<FunctionAST> ParseFunction(ClassAST*);
std::unique_ptr<IfBlockAST> ParseIf();
////////////////////////////////////////////////////////////////////////////////////
PrototypeAST *current_func_proto = nullptr;

void ParsePackageName(vector<string>& ret);

//parse basic type, will not get the array type
std::unique_ptr<Type> ParseBasicType()
{
	static std::unordered_set<Token> types = { tok_byte,tok_int,tok_long,tok_ulong,tok_uint,tok_float,tok_double,tok_boolean,tok_pointer };
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
	}
	else
	{
		if (types.find(tokenizer.CurTok) != types.end())
			type = make_unique<Type>(tokenizer.CurTok);
		else
			throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), "Expected an identifier or basic type name");
		tokenizer.GetNextToken();
	}
	return std::move(type);
}

std::unique_ptr<Type> ParseType()
{
	//CompileExpect(tok_as, "Expected \'as\'");
	if (tokenizer.CurTok != tok_as)
		return make_unique<Type>(tok_auto);
	tokenizer.GetNextToken(); //eat as
	std::unique_ptr<Type> type= ParseBasicType();
	while (tokenizer.CurTok == tok_left_index)
	{
		type->index_level++;
		tokenizer.GetNextToken();//eat [
		CompileExpect(tok_right_index, "Expected  \']\'");
	}
	return std::move(type);
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

std::unique_ptr<VariableDefAST> ParseDim(bool isglobal = false)
{
	std::vector<std::unique_ptr<VariableSingleDefAST>> defs;
	auto pos = tokenizer.GetSourcePos();
	auto def = ParseSingleDim();
	defs.push_back(std::move(def));
	for (;;)
	{

		switch (tokenizer.CurTok)
		{
		case tok_comma:
			tokenizer.GetNextToken();
			def = ParseSingleDim();
			defs.push_back(std::move(def));
			break;
		case tok_newline:
		case tok_eof:
		case tok_right_bracket:
			goto done;
			break;
		default:
			throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), "Expected a new line after variable definition");
		}

	}
done:
	if (isglobal)
	{
		for (auto&& v : defs)
		{
			CompileCheckGlobalConflict(v->Pos, v->name);
			std::reference_wrapper<const string> name = v->name;
			std::reference_wrapper<VariableSingleDefAST> ref = *v;
			cu.dimmap.insert(std::make_pair(name, ref));
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
	auto type = ParseBasicType();
	vector<std::unique_ptr<ExprAST>> expr;
	string method;
	while (tokenizer.CurTok == tok_left_index)
	{
		type->index_level++;
		tokenizer.GetNextToken();//eat [
		expr.push_back(ParseExpressionUnknown());
		CompileExpect(tok_right_index, "Expected  \']\'");
	}
	if (type->index_level == 0)
	{
		if (tokenizer.CurTok == tok_colon)
		{
			tokenizer.GetNextToken();//eat :
			CompileAssert(tokenizer.CurTok == tok_identifier, "Expected an identifier after :");
			method = tokenizer.IdentifierStr;
			tokenizer.GetNextToken();
		}
		if (tokenizer.CurTok == tok_left_bracket)
		{
			tokenizer.GetNextToken();//eat (
			expr = ParseArguments();
		}
	}
	return make_unique<NewExprAST>(std::move(type), std::move(expr), method, pos);
}

std::unique_ptr<ExprAST> ParsePrimaryExpression()
{
	std::unique_ptr<ExprAST> firstexpr;
	switch (tokenizer.CurTok)
	{
	case tok_address_of:
	case tok_pointer_of:
	{
		Token tok = tokenizer.CurTok;
		SourcePos pos = tokenizer.GetSourcePos();
		tokenizer.GetNextToken();
		CompileExpect(tok_left_bracket, "Expected \"(\" after addressof");
		firstexpr = ParseExpressionUnknown();
		CompileExpect(tok_right_bracket, "Expected \')\'");
		firstexpr = make_unique<AddressOfExprAST>(std::move(firstexpr),tok== tok_address_of, pos);
		break;
	}
	case tok_new:
	{
		firstexpr = ParseNew();
		break;
	}
	case tok_this:
		firstexpr = make_unique<ThisExprAST>();
		tokenizer.GetNextToken();
		break;
	case tok_null:
		firstexpr = make_unique<NullExprAST>();
		tokenizer.GetNextToken();
		break;
	case tok_identifier:
		firstexpr = make_unique<IdentifierExprAST>(tokenizer.IdentifierStr);
		tokenizer.GetNextToken();
		break;
	case tok_number:
		firstexpr = make_unique<NumberExprAST>(tokenizer.NumVal);
		tokenizer.GetNextToken();
		break;
	case tok_string_literal:
		firstexpr = make_unique<StringLiteralAST>(tokenizer.IdentifierStr);
		tokenizer.GetNextToken();
		break;
	case tok_left_bracket:
		tokenizer.GetNextToken();
		firstexpr = ParseExpressionUnknown();
		CompileExpect(tok_right_bracket, "Expected \')\'");
		break;
	case tok_func:
		tokenizer.GetNextToken(); //eat function
		firstexpr = ParseFunction(nullptr);
		CompileAssert(tok_newline == tokenizer.CurTok, "Expected newline after function definition");
		return firstexpr; //don't eat the newline token!
		break;
	case tok_eof:
		return nullptr;
		break;
	default:
		throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), "Expected an expression");
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
			tokenizer.GetNextToken();//eat [
			firstexpr = make_unique<IndexExprAST>(std::move(firstexpr), CompileExpectNotNull(ParseExpressionUnknown(), "Expected an expression for index"), pos);
			CompileExpect(tok_right_index, "Expected  \']\'");
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
		return -1;
		break;
	default:
		auto f = tok_precedence.find(tok);
		if (f != tok_precedence.end())
		{
			return f->second;
		}
		throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), "Unknown Token");
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
std::unique_ptr<ExprAST> ParseExpressionUnknown()
{

	return ParseBinOpRHS(0,
		CompileExpectNotNull(ParsePrimaryExpression(), "Expected an expression"));
}




void ParseBasicBlock(ASTBasicBlock& body, Token optional_tok)
{
	std::unique_ptr<ExprAST> firstexpr;
	SourcePos pos(0, 0);
	while (true)
	{

		switch (tokenizer.CurTok)
		{
		case tok_newline:
			tokenizer.GetNextToken(); //eat newline
			continue;
			break;
		case tok_dim:
			tokenizer.GetNextToken(); //eat dim
			body.body.push_back(std::move(ParseDim()));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after variable definition");
			break;
		case tok_return:
			tokenizer.GetNextToken(); //eat return
			pos = tokenizer.GetSourcePos();
			assert(current_func_proto && "Current func proto is empty!");
			body.body.push_back(make_unique<ReturnAST>(ParseExpressionUnknown(), current_func_proto, pos));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line");
			break;
		case tok_func:
			tokenizer.GetNextToken(); //eat function
			body.body.push_back(std::move(ParseFunction(nullptr)));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after function definition");
			break;
		case tok_end:
			tokenizer.GetNextToken(); //eat end
			if (optional_tok >= 0 && tokenizer.CurTok == optional_tok) //optional: end function/if/for...
				tokenizer.GetNextToken(); //eat it
			goto done;
			break;
		case tok_else:
			//don't eat else here
			goto done;
			break;
		case tok_eof:
			throw CompileError("Unexpected end of file, missing \"end\"");
			break;
		case tok_if:
			tokenizer.GetNextToken(); //eat if
			body.body.push_back(std::move(ParseIf()));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after if-block");
			break;
		default:
			firstexpr = ParseExpressionUnknown();
			CompileExpect({ tok_eof,tok_newline }, "Expect a new line after expression");
			CompileAssert(firstexpr != nullptr, "Compiler internal error: firstexpr=null");
			body.body.push_back(std::move(firstexpr));
		}
	}
done:
	return;
}
std::unique_ptr<IfBlockAST> ParseIf()
{
	SourcePos pos = tokenizer.GetSourcePos();
	std::unique_ptr<ExprAST> cond = ParseExpressionUnknown();
	CompileExpect(tok_then, "Expected \"then\" after the condition of \"if\"");
	CompileExpect(tok_newline, "Expected a newline after \"then\"");
	ASTBasicBlock true_block;
	ASTBasicBlock false_block;
	ParseBasicBlock(true_block, tok_if);
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
	CompileAssert(tokenizer.CurTok == tok_newline, "Expected a new line after \"if\" block");
	return make_unique<IfBlockAST>(std::move(cond), std::move(true_block), std::move(false_block), pos);
}

std::unique_ptr<FunctionAST> ParseFunction(ClassAST* cls)
{
	auto pos = tokenizer.GetSourcePos();
	std::string name;
	if (tokenizer.CurTok == tok_identifier)
	{
		name = tokenizer.IdentifierStr;
		tokenizer.GetNextToken();
	}
	CompileExpect(tok_left_bracket, "Expected \'(\'");
	std::unique_ptr<VariableDefAST> args;
	if (tokenizer.CurTok != tok_right_bracket)
		args = ParseDim();
	CompileExpect(tok_right_bracket, "Expected \')\'");
	auto rettype = ParseType();
	if (rettype->type == tok_auto)
		rettype->type = tok_void;
	auto funcproto = make_unique<PrototypeAST>(name, std::move(args), std::move(rettype), cls, tokenizer.GetSourcePos());
	current_func_proto = funcproto.get();

	ASTBasicBlock body;

	//parse function body
	ParseBasicBlock(body, tok_func);
	current_func_proto = nullptr;
	return make_unique<FunctionAST>(std::move(funcproto), std::move(body), pos);
}
std::unique_ptr<FunctionAST> ParseDeclareFunction(ClassAST* cls)
{
	auto pos = tokenizer.GetSourcePos();
	std::string name = tokenizer.IdentifierStr;
	CompileExpect(tok_identifier, "Expected an identifier");
	CompileExpect(tok_left_bracket, "Expected \'(\'");
	std::unique_ptr<VariableDefAST> args;
	if (tokenizer.CurTok != tok_right_bracket)
		args = ParseDim();
	CompileExpect(tok_right_bracket, "Expected \')\'");
	auto rettype = ParseType();
	CompileExpect({ tok_newline, tok_eof }, "Expected a new line after function declaration");
	if (rettype->type == tok_auto)
		rettype->type = tok_void;
	auto funcproto = make_unique<PrototypeAST>(name, std::move(args), std::move(rettype), cls, tokenizer.GetSourcePos());
	return make_unique<FunctionAST>(std::move(funcproto), pos);
}


std::unique_ptr<ClassAST> ParseClass()
{
	auto pos = tokenizer.GetSourcePos();
	std::string name = tokenizer.IdentifierStr;
	CompileExpect(tok_identifier, "Expected an identifier for class name");
	CompileExpect(tok_newline, "Expected an newline after class name");
	std::unique_ptr<ClassAST> ret = make_unique<ClassAST>(name, pos);
	std::vector<FieldDef>& fields = ret->fields;
	std::vector<MemberFunctionDef>& funcs = ret->funcs;
	unordered_map<reference_wrapper<const string>, int>& fieldmap = ret->fieldmap;
	unordered_map<reference_wrapper<const string>, int>& funcmap = ret->funcmap;
	auto classname_checker = [&fieldmap, &funcmap, &fields, &funcs](SourcePos pos, const string& name)
	{
		auto prv1 = fieldmap.find(name);
		if (prv1 != fieldmap.end())
		{
			throw CompileError(pos.line, pos.pos, string("The name ") + name + " is already used in " + fields[prv1->second].decl->Pos.ToString());
		}
		auto prv2 = funcmap.find(name);
		if (prv2 != funcmap.end())
		{
			throw CompileError(pos.line, pos.pos, string("The name ") + name + " is already used in " + funcs[prv2->second].decl->Pos.ToString());
		}
	};
	while (true)
	{
		AccessModifier access;
		switch (tokenizer.CurTok)
		{
		case tok_newline:
			tokenizer.GetNextToken(); //eat newline
			continue;
			break;
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
				CompileExpect(tok_newline, "Expected a new line after variable definition");
			}
			else if (tokenizer.CurTok == tok_func)
			{
				tokenizer.GetNextToken(); //eat function
				funcs.push_back(MemberFunctionDef(access, ParseFunction(ret.get())));
				classname_checker(funcs.back().decl->Pos, funcs.back().decl->GetName());
				funcmap.insert(std::make_pair(reference_wrapper<const string>(funcs.back().decl->GetName()),
					funcs.size() - 1));
				CompileExpect(tok_newline, "Expected a new line after function definition");
			}
			else
			{
				throw CompileError("After public/private, only accept function/variable definitions");
			}
			break;
		case tok_eof:
			throw CompileError("Unexpected end of file, missing \"end\"");
			break;
		case tok_func:
			tokenizer.GetNextToken(); //eat function
			funcs.push_back(MemberFunctionDef(access_private, ParseFunction(ret.get())));
			classname_checker(funcs.back().decl->Pos, funcs.back().decl->GetName());
			funcmap.insert(std::make_pair(reference_wrapper<const string>(funcs.back().decl->GetName()),
				funcs.size() - 1));
			CompileExpect({ tok_newline }, "Expected a new line after function definition");
			break;
		case tok_declare:
			tokenizer.GetNextToken(); //eat declare
			CompileExpect(tok_func, "Expected a function after declare");
			funcs.push_back(MemberFunctionDef(access_private, ParseDeclareFunction(ret.get())));
			classname_checker(funcs.back().decl->Pos, funcs.back().decl->GetName());
			funcmap.insert(std::make_pair(reference_wrapper<const string>(funcs.back().decl->GetName()),
				funcs.size() - 1));
			break;
		case tok_end:
			tokenizer.GetNextToken(); //eat end
			if (tokenizer.CurTok == tok_class) //optional: end function
				tokenizer.GetNextToken(); //eat function
			goto done;
			break;
		default:
			throw CompileError("Expected member declarations");
		}
	}
done:
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

string GetModuleNameByArray(const vector<string>& package)
{
	string ret=package[0];
	for (int i = 1; i < package.size(); i++)
	{
		ret += '.';
		ret += package[i];
	}
	return ret;
}

ImportedModule* DoImportPackage(const vector<string>& package)
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
void InsertName(const string& name,ClassAST* ptr)
{
	auto itr2 = cu.imported_classmap.find(name);
	WarnAssert(itr2 == cu.imported_classmap.end(),
		string("The imported class ") + ptr->GetUniqueName()
		+ " has overwritten the previously imported class " + itr2->second->GetUniqueName());
	if(itr2!= cu.imported_classmap.end()) cu.imported_classmap.erase(itr2);
	cu.imported_classmap[strref(name)] = ptr;
}
void InsertName(const string& name, VariableSingleDefAST* ptr)
{
	auto itr2 = cu.imported_dimmap.find(name);
	WarnAssert(itr2 == cu.imported_dimmap.end(),
		string("The imported variable ") + ptr->name
		+ " has overwritten the previously imported variable.");
	if (itr2 != cu.imported_dimmap.end()) cu.imported_dimmap.erase(itr2);
	cu.imported_dimmap.insert(std::make_pair(strref(name), ptr));
}


void InsertName(const string& name, FunctionAST* ptr)
{
	auto itr2 = cu.imported_funcmap.find(name);
	WarnAssert(itr2 == cu.imported_funcmap.end(),
		string("The imported function ") + ptr->GetName()
		+ " has overwritten the previously imported function.");
	if (itr2 != cu.imported_funcmap.end()) cu.imported_funcmap.erase(itr2);
	cu.imported_funcmap.insert(std::make_pair(strref(name), ptr));
}
void DoImportPackageAll(const vector<string>& package)
{
	auto mod = DoImportPackage(package);
	for (auto& itr : mod->classmap)
	{
		InsertName(itr.first, itr.second.get());
	}
	for (auto& itr : mod->dimmap)
	{
		InsertName(itr.first, itr.second.get());
	}
	for (auto& itr : mod->funcmap)
	{
		InsertName(itr.first, itr.second.get());
	}
}

template<typename T>
bool FindAndInsertName(T& namemap, const string& name)
{
	auto dim = namemap.find(name);
	if (dim != namemap.end())
	{
		InsertName(dim->first, dim->second.get());
		return true;
	}
	return false;
}

void DoImportName(const vector<string>& package, const string& name)
{
	auto mod = DoImportPackage(package);
	if (FindAndInsertName(mod->dimmap, name)) return;
	if (FindAndInsertName(mod->funcmap, name)) return;
	if (FindAndInsertName(mod->classmap, name)) return;
	throw CompileError("Cannot find name " + name);
}

void AddAutoImport()
{
	DoImportPackageAll({"birdee"});
}

void ParseImports()
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
				DoImportPackage(package);
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
					DoImportName(package, name);
				}
				else if (tokenizer.CurTok == tok_mul)
				{
					tokenizer.GetNextToken();
					CompileExpect({ tok_newline,tok_eof }, "Expected a new line after import");
					DoImportPackageAll(package);
				}
			}
		}
		else
			break;
	}
}

int ParseTopLevel()
{
	std::vector<std::unique_ptr<StatementAST>>& out = cu.toplevel;
	std::unique_ptr<ExprAST> firstexpr;
	tokenizer.GetNextToken();
	while (tokenizer.CurTok == tok_newline)
		tokenizer.GetNextToken();

	if(!cu.is_corelib)
		AddAutoImport();
	ParsePackage();
	ParseImports();
	

	while (tokenizer.CurTok != tok_eof && tokenizer.CurTok != tok_error)
	{

		switch (tokenizer.CurTok)
		{
		case tok_newline:
			tokenizer.GetNextToken(); //eat newline
			continue;
			break;
		case tok_dim:
			tokenizer.GetNextToken(); //eat dim
			out.push_back(std::move(ParseDim(true)));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after variable definition");
			break;
		case tok_eof:
			return 0;
			break;
		case tok_func:
		{
			tokenizer.GetNextToken(); //eat function
			std::unique_ptr<FunctionAST> funcast = ParseFunction(nullptr);
			std::reference_wrapper<const string> funcname = funcast->GetName();
			std::reference_wrapper<FunctionAST> funcref = *funcast;
			CompileCheckGlobalConflict(funcast->Pos, funcname);
			cu.funcmap.insert(std::make_pair(funcname, funcref));
			out.push_back(std::move(funcast));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after function definition");
			break;
		}
		case tok_declare:
		{
			tokenizer.GetNextToken(); //eat declare
			CompileExpect(tok_func, "Expected a function after declare");
			std::unique_ptr<FunctionAST> funcast = ParseDeclareFunction(nullptr);
			std::reference_wrapper<const string> funcname = funcast->GetName();
			std::reference_wrapper<FunctionAST> funcref = *funcast;
			CompileCheckGlobalConflict(funcast->Pos, funcname);
			cu.funcmap.insert(std::make_pair(funcname, funcref));
			out.push_back(std::move(funcast));
			break;
		}
		case tok_if:
			tokenizer.GetNextToken(); //eat if
			out.push_back(std::move(ParseIf()));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after if-block");
			break;
		case tok_class:
		{
			tokenizer.GetNextToken(); //eat class
			auto classdef = ParseClass();
			std::reference_wrapper<const string> clscname = classdef->name;
			std::reference_wrapper<ClassAST> classref = *classdef;
			CompileCheckGlobalConflict(classdef->Pos, clscname);
			cu.classmap.insert(std::make_pair(clscname, classref));
			out.push_back(std::move(classdef));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after class definition");
			break;
		}

		default:
			firstexpr = ParseExpressionUnknown();
			CompileExpect({ tok_eof,tok_newline }, "Expect a new line after expression");
			//if (!firstexpr)
			//	break;
			CompileAssert(firstexpr != nullptr, "Compiler internal error: firstexpr=null");
			out.push_back(std::move(firstexpr));
		}
	}

	return 0;
}