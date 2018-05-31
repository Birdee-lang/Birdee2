#include "AST.h"
#include "CompileError.h"
#include "SourcePos.h"
#include <cassert>
#include "CastAST.h"
#include <sstream>
using std::unordered_map;
using std::string;
using std::reference_wrapper;

using namespace Birdee;

class ScopeManager
{
public:
	typedef unordered_map<reference_wrapper<const string>, VariableSingleDefAST*> BasicBlock;
	vector<ClassAST*> class_stack;
	vector <BasicBlock> basic_blocks;

	inline bool IsCurrentClass(ClassAST* cls)
	{
		return class_stack.size() > 0 && class_stack.back() == cls;
	}

	FieldDef* FindFieldInClass(const string& name)
	{
		if (class_stack.size() > 0)
		{
			ClassAST* cls = class_stack.back();
			auto cls_field = cls->fieldmap.find(name);
			if (cls_field != cls->fieldmap.end())
			{
				return &(cls->fields[cls_field->second]);
			}
		}
		return nullptr;
	}

	MemberFunctionDef* FindFuncInClass(const string& name)
	{
		if (class_stack.size() > 0)
		{
			ClassAST* cls = class_stack.back();
			auto func_field = cls ->funcmap.find(name);
			if (func_field != cls->funcmap.end())
			{
				return &(cls->funcs[func_field->second]);
			}
		}
		return nullptr;
	}

	VariableSingleDefAST* FindLocalVar(const string& name)
	{
		if (basic_blocks.size() > 0)
		{
			for (auto itr = basic_blocks.rbegin(); itr != basic_blocks.rend(); itr++)
			{
				auto var = itr->find(name);
				if (var != itr->end())
				{
					return var->second;
				}
			}
		}
		return nullptr;
	}

	void PushBasicBlock()
	{
		basic_blocks.push_back(BasicBlock());
	}
	void PopBasicBlock()
	{
		basic_blocks.pop_back();
	}
	void PushClass(ClassAST* cls)
	{
		class_stack.push_back(cls);
	}
	void PopClass()
	{
		class_stack.pop_back();
	}

	unique_ptr<ResolvedIdentifierExprAST> ResolveName(const string& name,SourcePos pos)
	{
		auto bb = FindLocalVar(name);
		if (bb)
		{
			return make_unique<LocalVarExprAST>(bb,pos);
		}
		auto cls_field = FindFieldInClass(name);
		if (cls_field)
		{
			return make_unique<MemberExprAST>(make_unique<ThisExprAST>(class_stack.back(),pos), cls_field,pos);
		}
		auto func_field = FindFuncInClass(name);
		if (func_field)
		{
			return make_unique<MemberExprAST>(make_unique<ThisExprAST>(class_stack.back(),pos), func_field,pos);
		}
		/*if (basic_blocks.size() != 1) // top-level variable finding disabled
		{
			auto global_dim = cu.dimmap.find(name);
			if (global_dim != cu.dimmap.end())
			{
				return make_unique<LocalVarExprAST>(&(global_dim->second.get()));
			}
		}*/
		auto func = cu.funcmap.find(name);
		if (func != cu.funcmap.end())
		{
			return make_unique<ResolvedFuncExprAST>(&(func->second.get()),pos);
		}
		throw CompileError(pos.line, pos.pos, "Cannot resolve name: " + name);
		return nullptr;
	}

}scope_mgr;

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
		return make_unique<CastNumberExpr<tok_int, typeto>>(std::move(val),pos);
	case tok_long:
		return make_unique<CastNumberExpr<tok_long, typeto>>(std::move(val),pos);
	case tok_ulong:
		return make_unique<CastNumberExpr<tok_ulong, typeto>>(std::move(val),pos);
	case tok_uint:
		return make_unique<CastNumberExpr<tok_uint, typeto>>(std::move(val),pos);
	case tok_float:
		return make_unique<CastNumberExpr<tok_float, typeto>>(std::move(val),pos);
	case tok_double:
		return make_unique<CastNumberExpr<tok_double, typeto>>(std::move(val),pos);

	}
	ThrowCastError(target, val->resolved_type, pos);
	return nullptr;
}

void FixNull(ExprAST* v, ResolvedType& target)
{
	NullExprAST* expr = dynamic_cast<NullExprAST*>(v);
	assert(expr);
	expr->resolved_type = target;
}

unique_ptr<ExprAST> FixTypeForAssignment(ResolvedType& target, unique_ptr<ExprAST>&& val,SourcePos pos)
{
	if (target == val->resolved_type)
	{
		return std::move(val);
	}
	if ( target.isReference() && val->resolved_type.isNull())
	{
		FixNull(val.get(), target);
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


Token PromoteNumberExpression(unique_ptr<ExprAST>& v1, unique_ptr<ExprAST>& v2,bool isBool, SourcePos pos)
{
	static unordered_map<Token, int> promotion_map = {
	{tok_int,0},
	{tok_ulong,1},
	{tok_long,2},
	{tok_ulong,3},
	{tok_float,4},
	{tok_double,5},
	};
	int p1 = promotion_map[v1->resolved_type.type];
	int p2 = promotion_map[v2->resolved_type.type];
	if (p1 == p2)
		return isBool ? tok_boolean: v1->resolved_type.type;
	else if (p1 > p2)
	{
		v2 = FixTypeForAssignment(v1->resolved_type, std::move(v2), pos);
		return isBool ? tok_boolean : v1->resolved_type.type;
	}
	else
	{
		v1 = FixTypeForAssignment(v2->resolved_type, std::move(v1), pos);
		return isBool ? tok_boolean : v2->resolved_type.type;
	}

}


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
			this->type = tok_class;
			this->class_ast = GetItemByName(cu.classmap,ty->name,pos);
			//fix-me: should find function proto
		}

	}

	void CompileUnit::Phase0()
	{
		for (auto& node : funcmap)
		{
			node.second.get().Phase0();
		}
		for (auto& node : classmap)
		{
			node.second.get().Phase0();
		}
	}

	void CompileUnit::Phase1()
	{
		scope_mgr.PushBasicBlock();
		for (auto& stmt : toplevel)
		{
			stmt->Phase1();
		}
		scope_mgr.PopBasicBlock();
	}
	string ResolvedType::GetString()
	{
		if (type == tok_class)
			return GetClassASTName(class_ast);
		if (type == tok_null)
			return "null_t";
		if (type == tok_func)
			return "func";
		if (type == tok_void)
			return "void";
		if (isTypeToken(type))
		{
			std::stringstream buf;
			buf << GetTokenString(type);
			for(int i=0;i<index_level;i++)
				buf<<"[]";
			return buf.str();
		}
		else
			return "(Error type)";
	}
	bool operator==(const PrototypeAST& ths, const PrototypeAST& other) 
	{
		assert(ths.resolved_type.isResolved() && other.resolved_type.isResolved());
		if (!(ths.resolved_type == other.resolved_type))
			return false;
		if (ths.resolved_args.size() != other.resolved_args.size())
			return false;
		auto itr1 = ths.resolved_args.begin();
		auto itr2 = other.resolved_args.begin();
		for (; itr1 != ths.resolved_args.end(); itr1++, itr2++)
		{
			ResolvedType& t1 = (*itr1)->resolved_type;
			ResolvedType& t2 = (*itr2)->resolved_type;
			assert(t1.isResolved() && t2.isResolved());
			if (!(t1 == t2))
				return false;
		}
		return true;

	}

	void AddressOfExprAST::Phase1()
	{
		expr->Phase1();
		CompileAssert(expr->resolved_type.isReference(), Pos, "The expression in addressof should be a reference type");
		resolved_type.type = tok_pointer;
	}

	void IndexExprAST::Phase1()
	{
		Expr->Phase1();
		CompileAssert(Expr->resolved_type.index_level > 0, Pos, "The indexed expression should be indexable");
		Index->Phase1();
		CompileAssert(Index->resolved_type.isInteger(), Pos, "The index should be an integer");
		resolved_type = Expr->resolved_type;
		resolved_type.index_level--;
	}

	void IdentifierExprAST::Phase1()
	{
		impl = scope_mgr.ResolveName(Name, Pos);
		resolved_type = impl->resolved_type;
	}

	void ASTBasicBlock::Phase1()
	{
		scope_mgr.PushBasicBlock();
		for (auto&& node : body)
		{
			node->Phase1();
		}
		scope_mgr.PopBasicBlock();
	}

	void ASTBasicBlock::Phase1(PrototypeAST* proto)
	{
		scope_mgr.PushBasicBlock();
		proto->Phase1();
		for (auto&& node : body)
		{
			node->Phase1();
		}
		scope_mgr.PopBasicBlock();
	}

	void VariableSingleDefAST::Phase1()
	{
		Phase0();
		if (resolved_type.type == tok_auto)
		{
			CompileAssert(val.get(), Pos, "dim with no type must have an initializer");
			val->Phase1();
			resolved_type = val->resolved_type;
		}
		else
		{
			if (val.get())
			{
				val->Phase1();
				val = FixTypeForAssignment(resolved_type, std::move(val), Pos);
			}
		}
		scope_mgr.basic_blocks.back()[name] = this;
	}

	void FunctionAST::Phase1()
	{
		Phase0();
		Body.Phase1(Proto.get());
	}

	void VariableSingleDefAST::Phase1InClass()
	{
		Phase0();
		if (resolved_type.type == tok_auto)
		{
			throw CompileError(Pos.line, Pos.pos, "Member field of class must be defined with a type");
		}
		else
		{
			if (val.get())
			{
				throw CompileError(Pos.line, Pos.pos, "Member field of class cannot have initializer");
				//val->Phase1();
				//val = FixTypeForAssignment(resolved_type, std::move(val), Pos);
			}
		}
	}

	void ClassAST::Phase1()
	{
		Phase0();
		scope_mgr.PushClass(this);
		for (auto& fielddef : fields)
		{
			fielddef.decl->Phase1InClass();
		}
		for (auto& funcdef : funcs)
		{
			funcdef.decl->Phase1();
		}
		scope_mgr.PopClass();
	}
	void MemberExprAST::Phase1()
	{
		if (resolved_type.isResolved())
			return;
		Obj->Phase1();
		CompileAssert(Obj->resolved_type.type == tok_class, Pos, "The expression before the member should be an object");
		ClassAST* cls = Obj->resolved_type.class_ast;
		auto field = cls->fieldmap.find(member);
		if (field != cls->fieldmap.end())
		{
			if (cls->fields[field->second].access == access_private && !scope_mgr.IsCurrentClass(cls)) // if is private and we are not in the class
				throw CompileError(Pos.line, Pos.pos, "Accessing a private member outside of a class");
			kind = member_field;
			this->field = &(cls->fields[field->second]);
			resolved_type = this->field->decl->resolved_type;
			return;
		}
		auto func = cls->funcmap.find(member);
		if (func != cls->funcmap.end())
		{
			kind = member_function;
			this->func = &(cls->funcs[func->second]);
			if (this->func->access == access_private && !scope_mgr.IsCurrentClass(cls)) // if is private and we are not in the class
				throw CompileError(Pos.line, Pos.pos, "Accessing a private member outside of a class");
			resolved_type = this->func->decl->resolved_type;
			return;
		}
		throw CompileError(Pos.line, Pos.pos, "");
	}

	void CallExprAST::Phase1()
	{
		Callee->Phase1();
		CompileAssert(Callee->resolved_type.type == tok_func, Pos, "The expression should be callable");
		auto proto = Callee->resolved_type.proto_ast;
		std::stringstream buf;
		buf << "The function requires " << proto->resolved_args.size() << " Arguments, but " << Args.size() << "are given";
		CompileAssert(proto->resolved_args.size() == Args.size(), Pos, buf.str());
		int i = 0;
		for (auto& arg : Args)
		{
			arg->Phase1();
			SourcePos pos = arg->Pos;
			arg = FixTypeForAssignment(proto->resolved_args[i]->resolved_type, std::move(arg), pos);
			i++;
		}
		resolved_type = proto->resolved_type;
	}

	void ThisExprAST::Phase1()
	{
		CompileAssert(scope_mgr.class_stack.size() > 0, Pos, "Cannot reference \"this\" outside of a class");
		resolved_type.type = tok_class;
		resolved_type.class_ast = scope_mgr.class_stack.back();
	}

	void ResolvedFuncExprAST::Phase1()
	{
		def->Phase0(); //fix-me: maybe don't need to call phase0?
		resolved_type = def->resolved_type;
	}

	void LocalVarExprAST::Phase1()
	{
		def->Phase0();
		resolved_type = def->resolved_type;
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
		resolved_type.type = tok_class;
		resolved_type.class_ast = &string_cls;	
	}

	void IfBlockAST::Phase1()
	{
		cond->Phase1();
		CompileAssert(cond->resolved_type.type == tok_boolean && cond->resolved_type.index_level == 0,
			Pos, "The condition of \"if\" expects a boolean expression");
		iftrue.Phase1();
		iffalse.Phase1();
	}

	void BinaryExprAST::Phase1()
	{
		LHS->Phase1();
		RHS->Phase1();
		if (Op == tok_assign)
		{
			if (IdentifierExprAST* idexpr = dynamic_cast<IdentifierExprAST*>(LHS.get()))
				CompileAssert(idexpr->impl->isMutable(), Pos, "Cannot assign to an immutable value");
			else if (MemberExprAST* memexpr = dynamic_cast<MemberExprAST*>(LHS.get()))
				CompileAssert(memexpr->isMutable(), Pos, "Cannot assign to an immutable value");
			else if (instance_of<IndexExprAST>(LHS.get()))
			{}
			else
				throw CompileError(Pos.line, Pos.pos, "The left vaule of the assignment is not an variable");
			RHS = FixTypeForAssignment(LHS->resolved_type, std::move(RHS), Pos);
			resolved_type.type = tok_void;
			return;
		}
		if (Op == tok_equal || Op == tok_ne)
		{
			if (LHS->resolved_type == RHS->resolved_type)
				resolved_type.type = tok_boolean;
			else if (LHS->resolved_type.isNull() && RHS->resolved_type.isReference())
			{
				FixNull(LHS.get(), RHS->resolved_type);
				resolved_type.type = tok_boolean;
			}
			else if (RHS->resolved_type.isNull() && LHS->resolved_type.isReference())
			{
				FixNull(RHS.get(), LHS->resolved_type);
				resolved_type.type = tok_boolean;
			}
			else
				resolved_type.type=PromoteNumberExpression(LHS, RHS, true, Pos);
		}
		else
		{
			CompileAssert(LHS->resolved_type.isNumber() && RHS->resolved_type.isNumber(), Pos, "Currently only binary expressions of Numbers are supported");
			resolved_type.type = PromoteNumberExpression(LHS, RHS, isBooleanToken(Op), Pos);
		}

	}


}