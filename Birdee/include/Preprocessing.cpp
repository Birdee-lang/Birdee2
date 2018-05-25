#include "AST.h"
#include "CompileError.h"
#include "SourcePos.h"
#include <cassert>
#include "CastAST.h"

using std::unordered_map;
using std::string;
using std::reference_wrapper;

using namespace Birdee;

template <typename T>
T* GetItemByName(const unordered_map<reference_wrapper<const string>, reference_wrapper<T>>& M,
	const string& name, SourcePos pos)
{
	auto itr = M.find(name);
	if (itr == M.end())
		throw CompileError(pos.line, pos.pos, "Cannot find the name: " + name);
	return &(itr->second.get());
}

inline void CompileAssert(bool a, SourcePos pos,const std::string& msg)
{
	if (!a)
	{
		throw CompileError(pos.line, pos.pos, msg);
	}
}


void ThrowCastError(ResolvedType& target, ResolvedType& fromtype, SourcePos pos)
{
	string msg = "Cannot cast from type ";
	msg += fromtype.GetString();
	msg += " to type ";
	msg += target.GetString();
	throw CompileError(pos.line, pos.pos, msg);
}

template <Token typeto>
unique_ptr<ExprAST> FixTypeForAssignment2(ResolvedType& target, unique_ptr<ExprAST>&& val, SourcePos pos)
{
	switch (val->resolved_type.type)
	{
	case tok_int:
		return make_unique<CastNumberExpr<tok_int, typeto>>(std::move(val));
	case tok_long:
		return make_unique<CastNumberExpr<tok_long, typeto>>(std::move(val));
	case tok_ulong:
		return make_unique<CastNumberExpr<tok_ulong, typeto>>(std::move(val));
	case tok_uint:
		return make_unique<CastNumberExpr<tok_uint, typeto>>(std::move(val));
	case tok_float:
		return make_unique<CastNumberExpr<tok_float, typeto>>(std::move(val));
	case tok_double:
		return make_unique<CastNumberExpr<tok_double, typeto>>(std::move(val));

	}
	ThrowCastError(target, val->resolved_type, pos);
	return nullptr;
}

unique_ptr<ExprAST> FixTypeForAssignment(ResolvedType& target, unique_ptr<ExprAST>&& val,SourcePos pos)
{
	if (target == val->resolved_type)
	{
		return std::move(val);
	}
#define fix_type(typeto) FixTypeForAssignment2<typeto>(target,std::move(val),pos)
	else if (target.isNumber() && val->resolved_type.index_level==0)
	{
		switch (target.type)
		{
		case tok_int:
			return fix_type(tok_int);
		case tok_long:
			return fix_type(tok_long);
		case tok_ulong:
			return fix_type(tok_ulong);
		case tok_uint:
			return fix_type(tok_uint);
		case tok_float:
			return fix_type(tok_float);
		case tok_double:
			return fix_type(tok_double);

		}
	}
#undef fix_type
	ThrowCastError(target, val->resolved_type, pos);
	return nullptr;
}

//fix-me: should first resolve all the funtion's prototypes
namespace Birdee
{
	const string& GetClassASTName(ClassAST* cls)
	{
		return cls->name;
	}
	void ResolvedType::ResolveType(Type& type, SourcePos pos)
	{
		if (type.type == tok_identifier)
		{
			IdentifierType* ty = dynamic_cast<IdentifierType*>(&type);
			assert(ty && "Type should be a IdentifierType");
			this->class_ast = GetItemByName(cu.classmap,ty->name,pos);
		}
	}

	void NumberExprAST::Phase1()
	{
		resolved_type.type = Val.type;
	}

	void ReturnAST::Phase1()
	{
		Val->Phase1();
		assert(proto->resolved_type.type != tok_error && "The prototype should be resolved first");
		Val = FixTypeForAssignment(proto->resolved_type, std::move(Val),Pos);	
	}

	void StringLiteralAST::Phase1()
	{
		//fix-me: use the system package name of string
		static string name("string");
		static ClassAST& string_cls=cu.classmap.find(name)->second;
		resolved_type.type = tok_identifier;
		resolved_type.class_ast = &string_cls;	
	}
}