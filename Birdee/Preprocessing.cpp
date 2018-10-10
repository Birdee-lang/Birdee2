#include "BdAST.h"
#include "CompileError.h"
#include "SourcePos.h"
#include <cassert>
#include "CastAST.h"
#include <sstream>
#include "TemplateUtil.h"
using std::unordered_map;
using std::string;
using std::reference_wrapper;


using namespace Birdee;

//fix-me: show template stack on error

template <typename T>
T* FindImportByName(const unordered_map<reference_wrapper<const string>, T*>& M,
	const string& name)
{
	auto itr = M.find(name);
	if (itr == M.end())
		return nullptr;
	return (itr->second);
}

template <typename T>
T* FindImportByName(const unordered_map<string, std::unique_ptr<T>>& M,
	const string& name)
{
	auto itr = M.find(name);
	if (itr == M.end())
		return nullptr;
	return (itr->second.get());
}


class ScopeManager
{
public:
	typedef unordered_map<reference_wrapper<const string>, VariableSingleDefAST*> BasicBlock;
	vector<ClassAST*> class_stack;
	BasicBlock top_level_bb;
	vector <BasicBlock> basic_blocks;
	struct TemplateEnv
	{
		unordered_map<string, ResolvedType> typemap;
		unordered_map<string, ExprAST*> exprmap;
		SourcePos pos;
		bool isClass;
		ImportedModule* imported_mod;
		TemplateEnv():pos(0,0,0), isClass(false),imported_mod(nullptr){};
		TemplateEnv(SourcePos pos, bool isClass, ImportedModule* imported_mod): pos(pos), isClass(isClass) , imported_mod(imported_mod){}
		TemplateEnv& operator = (TemplateEnv&& v)
		{
			typemap = std::move(v.typemap);
			exprmap = std::move(v.exprmap);
			pos = v.pos;
			imported_mod = v.imported_mod;
			isClass = v.isClass;
			return *this;
		}
		TemplateEnv(TemplateEnv&& v):pos(0,0,0)
		{
			*this = std::move(v);
		}
	};
	vector<TemplateEnv> template_stack;
	vector<TemplateEnv> template_class_stack;
	typedef std::pair<vector<TemplateEnv>*, int> template_stack_frame;
	vector<template_stack_frame> template_trace_back_stack;

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

	unique_ptr<ResolvedIdentifierExprAST> FindAndCopyTemplateArgument(const string& name)
	{
		if (!template_stack.empty()) {
			auto itr = template_stack.back().exprmap.find(name);
			if (itr != template_stack.back().exprmap.end())
			{
				return unique_ptr_cast<ResolvedIdentifierExprAST>(itr->second->Copy());
			}
		}
		if (!template_class_stack.empty())
		{
			auto itr = template_class_stack.back().exprmap.find(name);
			if (itr != template_class_stack.back().exprmap.end())
			{
				return unique_ptr_cast<ResolvedIdentifierExprAST>(itr->second->Copy());
			}
		}
		return nullptr;
	}

	ResolvedType FindTemplateType(const string& name)
	{
		if (!template_stack.empty()) {
			auto itr = template_stack.back().typemap.find(name);
			if (itr != template_stack.back().typemap.end())
			{
				return itr->second;
			}
		}
		if (!template_class_stack.empty())
		{
			auto itr = template_class_stack.back().typemap.find(name);
			if (itr != template_class_stack.back().typemap.end())
			{
				return itr->second;
			}
		}
		return ResolvedType();
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

	TemplateEnv CreateTemplateEnv(const vector<TemplateArgument>& template_args,
		const vector<TemplateParameter>& parameters, bool isClass, ImportedModule* mod,SourcePos pos)
	{
		TemplateEnv env(pos, isClass,mod);
		for (int i = 0; i<template_args.size(); i++)
		{
			if (template_args[i].kind == TemplateArgument::TEMPLATE_ARG_TYPE)
				env.typemap[parameters[i].name] = template_args[i].type;
			else
				env.exprmap[parameters[i].name] = template_args[i].expr.get();
		}
		return std::move(env);
	}

	void SetTemplateEnv(const vector<TemplateArgument>& template_args,
		const vector<TemplateParameter>& parameters, ImportedModule* mod, SourcePos pos)
	{
		TemplateEnv env = CreateTemplateEnv(template_args, parameters,false, mod, pos);
		template_stack.push_back(std::move(env));
	}

	void RestoreTemplateEnv()
	{
		template_stack.pop_back();
	}

	void SetClassTemplateEnv(const vector<TemplateArgument>& template_args,
		const vector<TemplateParameter>& parameters, ImportedModule* mod, SourcePos pos)
	{
		TemplateEnv env = CreateTemplateEnv(template_args, parameters, true, mod, pos);
		template_class_stack.push_back(std::move(env));
	}

	void SetEmptyClassTemplateEnv()
	{
		template_class_stack.push_back(TemplateEnv());
	}

	void RestoreClassTemplateEnv()
	{
		template_class_stack.pop_back();
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

	bool isInTemplateOfOtherModule()
	{
		return (!template_stack.empty() && template_stack.back().imported_mod) 
			|| (!template_class_stack.empty() && template_class_stack.back().imported_mod);
	}

	VariableSingleDefAST* GetGlobalFromImportedTemplate(const string& name)
	{
		//we just need to check the module variable env of template stack for function
		auto check_template_module = [&](vector<TemplateEnv>& vec) -> Birdee::VariableSingleDefAST*
		{
			if (!vec.empty() && vec.back().imported_mod)
			{
				auto& dimmap = vec.back().imported_mod->dimmap;
				auto var = dimmap.find(name);
				if (var != dimmap.end())
				{
					return var->second.get();
				}
			}
			return nullptr;
		};
		auto v = check_template_module(template_stack);
		if (v)
			return v;
		return check_template_module(template_class_stack); //will return null if not in template
	}


	unique_ptr<ResolvedIdentifierExprAST> ResolveNameNoThrow(const string& name,SourcePos pos,ImportTree*& out_import)
	{
		auto bb = FindLocalVar(name);
		if (bb)
		{
			return make_unique<LocalVarExprAST>(bb,pos);
		}
		auto template_arg = FindAndCopyTemplateArgument(name);
		if (template_arg)
			return std::move(template_arg);

		bool isInOtherModule = isInTemplateOfOtherModule();
		if (isInOtherModule)
		{//if in template of another module, search global vars from the module
			auto ret = GetGlobalFromImportedTemplate(name);
			if (ret)
				return make_unique<LocalVarExprAST>(ret, pos);
		}
		else
		{
			auto global_itr = top_level_bb.find(name);
			if (global_itr != top_level_bb.end())
				return make_unique<LocalVarExprAST>(global_itr->second, pos);
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

		//if we are in a template && the template is imported from other modules
		auto check_template_module = [&](vector<TemplateEnv>& v)->unique_ptr<ResolvedFuncExprAST>
		{
			if (!template_stack.empty() && template_stack.back().imported_mod)
			{
				auto& funcmap = template_stack.back().imported_mod->funcmap;
				auto var = funcmap.find(name);
				if (var != funcmap.end())
				{
					return make_unique<ResolvedFuncExprAST>(var->second.get(), pos);
				}
			}
			return nullptr;
		};
		if (isInOtherModule)
		{
			auto module_func = check_template_module(template_stack);
			if (module_func)
				return std::move(module_func);
			else if (module_func = check_template_module(template_class_stack))
				return std::move(module_func);
		}
		else
		{
			auto func = cu.funcmap.find(name);
			if (func != cu.funcmap.end())
			{
				return make_unique<ResolvedFuncExprAST>(&(func->second.get()), pos);
			}
		}

		auto ret1 = FindImportByName(cu.imported_dimmap, name);
		if(ret1)
			return make_unique<LocalVarExprAST>(ret1, pos);
		auto ret2 = FindImportByName(cu.imported_funcmap, name);
		if (ret2)
			return make_unique<ResolvedFuncExprAST>(ret2, pos);
		
		auto package = cu.imported_packages.FindName(name);
		if (package)
		{
			out_import = package;
			return nullptr;
		}
		out_import = nullptr;
		return nullptr;
	}

	unique_ptr<ResolvedIdentifierExprAST> ResolveName(const string& name, SourcePos pos, ImportTree*& out_import)
	{
		unique_ptr<ResolvedIdentifierExprAST> ret = ResolveNameNoThrow(name, pos, out_import);
		if(!ret && !out_import)
			throw CompileError(pos.line, pos.pos, "Cannot resolve name: " + name);
		return ret;
	}
	BasicBlock& GetCurrentBasicBlock()
	{
		if (basic_blocks.empty())
			return top_level_bb;
		else
			return basic_blocks.back();
	}

};

struct PreprocessingState
{
	ScopeManager _scope_mgr;
	FunctionAST* _cur_func = nullptr;
	ClassAST* array_cls = nullptr;
	ClassAST* string_cls = nullptr;
};

#define scope_mgr (preprocessing_state._scope_mgr)
#define cur_func (preprocessing_state._cur_func)

static PreprocessingState preprocessing_state;
BD_CORE_API FunctionAST* GetCurrentPreprocessedFunction()
{
	return cur_func;
}

void ClearPreprocessingState()
{
	preprocessing_state.~PreprocessingState();
	new (&preprocessing_state) PreprocessingState();
}


std::string Birdee::GetTemplateStackTrace()
{
	std::stringstream buf;
	if (scope_mgr.template_trace_back_stack.size())
	{
		for (auto& pair : scope_mgr.template_trace_back_stack) 
		{
			auto& env = (*pair.first)[pair.second];
			buf << "At " << env.pos.ToString() << "\n"; //fix-me : print the types and the expressions
		}
	}
	return buf.str();
}

template <typename T,typename T2>
const T& GetItemByName(const unordered_map<T2, T>& M,
	const string& name, SourcePos pos)
{
	auto itr = M.find(name);
	if (itr == M.end())
		throw CompileError(pos.line, pos.pos, "Cannot find the name: " + name);
	return itr->second;
}


#define CompileAssert(a, p ,msg)\
do\
{\
	if (!(a))\
	{\
		throw CompileError(p.line, p.pos, msg);\
	}\
}while(0)\

static FunctionAST* GetFunctionFromExpression(ExprAST* expr,SourcePos Pos)
{
	FunctionAST* func = nullptr;
	IdentifierExprAST*  iden = dynamic_cast<IdentifierExprAST*>(expr);
	if (iden)
	{
		auto resolvedfunc = dynamic_cast<ResolvedFuncExprAST*>(iden->impl.get());
		if(resolvedfunc)
			func = resolvedfunc->def;
	}
	else
	{
		MemberExprAST*  mem = dynamic_cast<MemberExprAST*>(expr);
		if (mem)
		{
			if (mem->kind == MemberExprAST::member_function)
			{
				func = mem->func->decl.get();
			}
			else if (mem->kind == MemberExprAST::member_imported_function)
			{
				func = mem->import_func;
			}
		}
	}
	return func;
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
	case tok_byte:
		return make_unique<CastNumberExpr<tok_byte, typeto>>(std::move(val), pos);
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
		case tok_byte:
			return fix_type(tok_byte);
		}
	}
#undef fix_type
	ThrowCastError(target, val->resolved_type, pos);
	return nullptr;
}


Token PromoteNumberExpression(unique_ptr<ExprAST>& v1, unique_ptr<ExprAST>& v2,bool isBool, SourcePos pos)
{
	static unordered_map<Token, int> promotion_map = {
	{ tok_byte,-1 },
	{ tok_int,0 },
	{tok_uint,1},
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

extern string GetModuleNameByArray(const vector<string>& package);
namespace Birdee
{
	BD_CORE_API string GetClassASTName(ClassAST* cls)
	{
		return cls->GetUniqueName();
	}

	static void ParseRawTemplateArgs(vector<unique_ptr<ExprAST>>& raw_template_args, vector<TemplateArgument>& template_args)
	{
		auto& this_template_args = template_args;
		for (auto& template_arg : raw_template_args)
		{
			RunOnTemplateArg(template_arg.get(),
				[&this_template_args](BasicTypeExprAST* ex) {
				this_template_args.push_back(TemplateArgument(ResolvedType(*ex->type, ex->Pos)));
			},
				[&this_template_args](IdentifierExprAST* ex) {
				ImportTree* import_tree = nullptr;
				auto ret = scope_mgr.ResolveNameNoThrow(ex->Name, ex->Pos, import_tree);
				if (ret || import_tree)
					assert(0 && "Not implemented");
				IdentifierType type(ex->Name);
				Type& dummy = type;
				this_template_args.push_back(TemplateArgument(ResolvedType(dummy, ex->Pos)));
			},
				[&this_template_args](MemberExprAST* ex) {
				//fix-me: resolve this member!!!!!!!!!
				QualifiedIdentifierType type = QualifiedIdentifierType(ex->ToStringArray());
				Type& dummy = type;
				this_template_args.push_back(TemplateArgument(ResolvedType(dummy, ex->Pos)));
			},
				[&this_template_args](FunctionTemplateInstanceExprAST* ex) {
				if (instance_of<IdentifierExprAST>(ex->expr.get()))
				{
					IdentifierExprAST* iden = (IdentifierExprAST*)ex->expr.get();
					IdentifierType type(iden->Name);
					type.template_args = make_unique<vector<unique_ptr<ExprAST>>>(std::move(ex->raw_template_args));
					Type& dummy = type;
					this_template_args.push_back(TemplateArgument(ResolvedType(dummy, ex->Pos)));
				}
				else if (instance_of<MemberExprAST>(ex->expr.get()))
				{
					QualifiedIdentifierType type = QualifiedIdentifierType(((MemberExprAST*)ex->expr.get())->ToStringArray());
					type.template_args = make_unique<vector<unique_ptr<ExprAST>>>(std::move(ex->raw_template_args));
					Type& dummy = type;
					this_template_args.push_back(TemplateArgument(ResolvedType(dummy, ex->Pos)));
				}
				else
					assert(0 && "Not implemented");
			},
				[&this_template_args](IndexExprAST* ex) {
				if (instance_of<IdentifierExprAST>(ex->Expr.get()))
				{
					IdentifierType type(((IdentifierExprAST*)ex->Expr.get())->Name);
					type.template_args = make_unique<vector<unique_ptr<ExprAST>>>();
					type.template_args->push_back(std::move(ex->Index));
					Type& dummy = type;
					this_template_args.push_back(TemplateArgument(ResolvedType(dummy, ex->Pos)));
				}
				else if (instance_of<MemberExprAST>(ex->Expr.get()))
				{
					QualifiedIdentifierType type = QualifiedIdentifierType(((MemberExprAST*)ex->Expr.get())->ToStringArray());
					type.template_args = make_unique<vector<unique_ptr<ExprAST>>>();
					type.template_args->push_back(std::move(ex->Index));
					Type& dummy = type;
					this_template_args.push_back(TemplateArgument(ResolvedType(dummy, ex->Pos)));
				}
				else
					assert(0 && "Not implemented");
			},
				[&this_template_args, &template_arg](NumberExprAST* ex) {
				ex->Phase1();
				this_template_args.push_back(TemplateArgument(std::move(template_arg)));
			},
				[&this_template_args, &template_arg](StringLiteralAST* ex) {
				this_template_args.push_back(TemplateArgument(std::move(template_arg)));
			},
				[]() {
				throw CompileError("Invalid template argument expression type");
			}
			);
		}
	}

	void ResolvedType::ResolveType(Type& type, SourcePos pos)
	{
		if (type.type == tok_identifier)
		{
			IdentifierType* ty = dynamic_cast<IdentifierType*>(&type);
			assert(ty && "Type should be a IdentifierType");
			this->type = tok_class;
			auto rtype=scope_mgr.FindTemplateType(ty->name);
			if (rtype.type != tok_error)
			{
				*this = rtype;
				return;
			}
			//if we are in a template and it is imported from another modules
			if (!scope_mgr.template_stack.empty() && scope_mgr.template_stack.back().imported_mod)
			{
				auto& classmap = scope_mgr.template_stack.back().imported_mod->classmap;
				this->class_ast = GetItemByName(classmap, ty->name, pos).get();
			}
			else if (!scope_mgr.template_class_stack.empty() && scope_mgr.template_class_stack.back().imported_mod)
			{
				auto& classmap = scope_mgr.template_class_stack.back().imported_mod->classmap;
				this->class_ast = GetItemByName(classmap, ty->name, pos).get();
			}
			else
			{
				auto itr = cu.classmap.find(ty->name);
				if (itr == cu.classmap.end())
					this->class_ast = GetItemByName(cu.imported_classmap, ty->name, pos);
				else
					this->class_ast = &(itr->second.get());
				//fix-me: should find function proto
			}
		}
		else if (type.type == tok_package)
		{
			QualifiedIdentifierType* ty=dynamic_cast<QualifiedIdentifierType*>(&type);
			assert(ty && "Type should be a QualifiedIdentifierType");
			string clsname = ty->name.back();
			ty->name.pop_back();
			auto node = cu.imported_packages.Contains(ty->name);
			if (!node || (node && !node->mod))
			{
				throw CompileError(pos.line,pos.pos,"The module " + GetModuleNameByArray(ty->name) + " has not been imported");
			}
			auto itr = node->mod->classmap.find(clsname);
			if(itr== node->mod->classmap.end())
				throw CompileError(pos.line, pos.pos, "Cannot find class " + clsname+" in module "+ GetModuleNameByArray(ty->name));
			this->type = tok_class;
			this->class_ast = itr->second.get();
		}
		if (type.type == tok_identifier || type.type == tok_package)
		{
			GeneralIdentifierType* ty = static_cast<GeneralIdentifierType*>(&type);
			if (ty->template_args)
			{
				CompileAssert(class_ast->isTemplate(),pos, "The class must be a template class");
				vector<TemplateArgument>& arg= *(new vector<TemplateArgument>());
				auto& args = *ty->template_args.get();
				ParseRawTemplateArgs(args, arg);
				class_ast->template_param->ValidateArguments(arg, pos);
				class_ast = class_ast->template_param->GetOrCreate(&arg, class_ast, pos); //will take the ownership of the pointer arg
			}
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
		//scope_mgr.PushBasicBlock();
		for (auto& stmt : toplevel)
		{
			stmt->Phase1();
		}
		//scope_mgr.PopBasicBlock();
	}
	string ResolvedType::GetString() const
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

	BD_CORE_API bool operator==(const PrototypeAST& ths, const PrototypeAST& other)
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
	bool ResolvedType::operator<(const ResolvedType & that) const
	{
		if (type != that.type)
			return type < that.type;
		if (index_level != that.index_level)
			return index_level < that.index_level;
		if (type == tok_class)
			return class_ast < that.class_ast;
		if (type == tok_package)
			return import_node < that.import_node;
		if (type == tok_func)
			return proto_ast < that.proto_ast;
		return false;		
	}
	bool Birdee::Type::operator<(const Type & that)
	{
		if (type != that.type)
			return type < that.type;
		if (index_level != that.index_level)
			return index_level < that.index_level;
		if (type == tok_package)
		{
			QualifiedIdentifierType* vthis = (QualifiedIdentifierType*)this;
			QualifiedIdentifierType* vthat = (QualifiedIdentifierType*)&that;
			return vthis->name < vthat->name;
		}
		else if (type == tok_identifier)
		{
			IdentifierType* vthis = (IdentifierType*)this;
			IdentifierType* vthat = (IdentifierType*)&that;
			return vthis->name < vthat->name;
		}
		else
			return false; //equals
	}

	bool Birdee::NumberLiteral::operator<(const NumberLiteral & v)
	{
		if (type != v.type)
			return type < type;
		return v_ulong<v.v_ulong;
	}

#ifdef BIRDEE_USE_DYN_LIB
	static_assert(_WIN32, "Non-windows version is not implemented");
#include <Windows.h>
	static void Birdee_AnnotationStatementAST_Phase1(AnnotationStatementAST* ths)
	{
		
		typedef void(*PtrImpl)(AnnotationStatementAST* ths);
		static PtrImpl impl=nullptr;
		if (impl == nullptr)
		{
			auto hinst = LoadLibrary(L"BirdeeBinding.dll");
			assert(hinst);
			impl = (PtrImpl)GetProcAddress(hinst, "?Birdee_AnnotationStatementAST_Phase1@@YAXPEAVAnnotationStatementAST@Birdee@@@Z");
			assert(impl);
		}
		impl(ths);
	}
	static void Birdee_ScriptAST_Phase1(ScriptAST* ths)
	{
		typedef void(*PtrImpl)(ScriptAST* ths);
		static PtrImpl impl = nullptr;
		if (impl == nullptr)
		{
			auto hinst = LoadLibrary(L"BirdeeBinding.dll");
			assert(hinst);
			impl = (PtrImpl)GetProcAddress(hinst, "?Birdee_ScriptAST_Phase1@@YAXPEAVScriptAST@Birdee@@@Z");
			assert(impl);
		}
		impl(ths);
	}
#else
	extern void Birdee_AnnotationStatementAST_Phase1(AnnotationStatementAST* ths);
	extern void Birdee_ScriptAST_Phase1(ScriptAST* ths);
#endif

	void Birdee::ScriptAST::Phase1()
	{
		Birdee_ScriptAST_Phase1(this);
	}

	void Birdee::AnnotationStatementAST::Phase1()
	{
		Birdee_AnnotationStatementAST_Phase1(this);
	}

	bool Birdee::TemplateArgument::operator<(const TemplateArgument & that) const
	{
		if (kind != that.kind)
			return kind < that.kind;
		if (kind == TEMPLATE_ARG_TYPE)
			return type < that.type;
		//kind==TEMPLATE_ARG_EXPR
		//let NumberExprAST<StringLiteralAST
		NumberExprAST* n1 = dynamic_cast<NumberExprAST*>(expr.get()), *n2=dynamic_cast<NumberExprAST*>(that.expr.get());
		StringLiteralAST* s1 = dynamic_cast<StringLiteralAST*>(expr.get()), *s2 = dynamic_cast<StringLiteralAST*>(that.expr.get());
		if (n1)
		{
			if (s2)
				return true;
			if (n2)
				return n1->Val < n2->Val;
			assert(0 && "The expression is neither NumberExprAST nor StringLiteralAST");
		}
		else if(s1)
		{
			if (s2)
				return s1->Val < s2->Val;
			if (n2)
				return false; 
			assert(0 && "The expression is neither NumberExprAST nor StringLiteralAST");
		}
		assert(0 && "The expression is neither NumberExprAST nor StringLiteralAST");
		return false;
	}

	string int2str(const int v)
	{
		std::stringstream stream;
		stream << v;
		return stream.str();   
	}

	void DoTemplateValidateArguments(const vector<TemplateParameter>& params, const vector<TemplateArgument>& args, SourcePos Pos)
	{
		CompileAssert(params.size() == args.size(), Pos, 
			string("The template requires ") + int2str(params.size()) + " Arguments, but " + int2str(args.size()) + "are given");
		
		for (int i = 0; i < args.size(); i++)
		{
			if (params[i].isTypeParameter())
			{
				CompileAssert(args[i].kind == TemplateArgument::TEMPLATE_ARG_TYPE, Pos, string("Expected a type at the template parameter number ") + int2str(i + 1));
			}
			else
			{
				CompileAssert(args[i].kind == TemplateArgument::TEMPLATE_ARG_EXPR, Pos, string("Expected an expression at the template parameter number ") + int2str(i + 1));
				CompileAssert(instance_of<StringLiteralAST>(args[i].expr.get()) || instance_of<NumberExprAST>(args[i].expr.get()), Pos,
					string("Expected an constant expression at the template parameter number ") + int2str(i + 1));
				args[i].expr->Phase1();
				CompileAssert(ResolvedType(*params[i].type, Pos) == args[i].expr->resolved_type, Pos,
					string("The expression type does not match at the template parameter number ") + int2str(i + 1));
			}
		}
		
	}

	vector<string> Birdee::MemberExprAST::ToStringArray()
	{
		vector<string*> reverse;
		reverse.push_back(&member);
		ExprAST* cur = Obj.get();
		for(;;)
		{
			if (!instance_of<MemberExprAST>(cur))
			{
				CompileAssert(instance_of<IdentifierExprAST>(cur), cur->Pos, "Expected an identifier for template");
				reverse.push_back(&((IdentifierExprAST*)cur)->Name);
				break;
			}
			MemberExprAST* node = (MemberExprAST*)cur;
			reverse.push_back(&node->member);
			cur = node->Obj.get();
		}
		vector<string> ret;
		for (int i = reverse.size() - 1; i >= 0; i++)
		{
			ret.push_back(*reverse[i]);
		}
		return ret;
	}


	void FunctionTemplateInstanceExprAST::Phase1()
	{
		FunctionAST* func = nullptr;
		expr->Phase1();
		func = GetFunctionFromExpression(expr.get(),Pos);
		CompileAssert(func, Pos, "Expected a function name or a member function for template");
		CompileAssert(func->isTemplate(), Pos, "The function is not a template");
		ParseRawTemplateArgs(raw_template_args, template_args);
		raw_template_args.clear();
		func->template_param->ValidateArguments(template_args, Pos);
		instance = func->template_param->GetOrCreate(&template_args, func, Pos);
		resolved_type = instance->resolved_type;
	}
	static string GetTemplateArgumentString(const vector<TemplateArgument>& v)
	{
		std::stringstream buf;
		auto printit = [&buf](const TemplateArgument& arg,const string& suffix)
		{
			if (arg.kind == TemplateArgument::TEMPLATE_ARG_EXPR)
			{
				StringLiteralAST* str = dynamic_cast<StringLiteralAST*>(arg.expr.get());
				if (str)
					buf << '\"' << str->Val << '\"' << suffix;
				else
				{
					NumberExprAST* number = dynamic_cast<NumberExprAST*>(arg.expr.get());
					assert(number && "Template arg should be string or number constant");
					number->ToString(buf);
					buf << suffix;
				}
			}
			else
			{
				buf << arg.type.GetString() << suffix;
			}
		};
		buf << "[";
		for (int i=0;i<v.size()-1;i++)
			printit(v[i],",");
		printit(v.back(),"");
		buf << "]";
		return buf.str();
	}

	string Birdee::ClassAST::GetUniqueName()
	{
		if (package_name_idx == -1)
			return cu.symbol_prefix + name;
		else if (package_name_idx == -2)
			return name;
		else
			return cu.imported_module_names[package_name_idx] + '.' + name;
	}

	/*
	Will not Take the ownership of the pointer v
	*/
	static void Phase1ForTemplateInstance(FunctionAST* func, FunctionAST* src_func, const vector<TemplateArgument>& v,
		const vector<TemplateParameter>& parameters,ImportedModule* mod, SourcePos pos)
	{
		func->Proto->Name += GetTemplateArgumentString(v);
		ClassAST* cls_template=nullptr;
		if (func->Proto->cls) 
		{
			cls_template = func->Proto->cls;
			if (func->Proto->cls->template_instance_args)//if the function is defined in a template class, push the template environment
			{
				scope_mgr.SetClassTemplateEnv(*cls_template->template_instance_args,
					cls_template->template_source_class->template_param->params, mod, pos);
			}
			else
				scope_mgr.SetEmptyClassTemplateEnv();
			scope_mgr.PushClass(cls_template);
		}
		else
		{
			scope_mgr.SetEmptyClassTemplateEnv();
		}
		scope_mgr.SetTemplateEnv(v, parameters, mod, pos);
		scope_mgr.template_trace_back_stack.push_back(std::make_pair(&scope_mgr.template_stack, scope_mgr.template_stack.size() - 1));
		auto basic_blocks_backup = std::move(scope_mgr.basic_blocks);
		scope_mgr.basic_blocks = vector <ScopeManager::BasicBlock>();
		func->isTemplateInstance = true;
		func->Phase0();
		func->Phase1();
		scope_mgr.RestoreTemplateEnv();
		scope_mgr.template_trace_back_stack.pop_back();
		scope_mgr.basic_blocks = std::move(basic_blocks_backup);
		if (cls_template)
		{
			scope_mgr.PopClass();
		}
		scope_mgr.RestoreClassTemplateEnv();
	}

	/*
	Takes the ownership of the pointer v
	*/
	static void Phase1ForTemplateInstance(ClassAST* cls, ClassAST* src_cls, vector<TemplateArgument>& v,
		const vector<TemplateParameter>& parameters,ImportedModule* mod, SourcePos pos)
	{
		cls->template_instance_args = unique_ptr<vector<TemplateArgument>>(&v);
		cls->template_source_class = src_cls;
		cls->name += GetTemplateArgumentString(v);
		scope_mgr.SetClassTemplateEnv(v, parameters, mod, pos);
		scope_mgr.template_trace_back_stack.push_back(std::make_pair(&scope_mgr.template_class_stack, scope_mgr.template_class_stack.size() - 1));
		for (auto& funcdef : cls->funcs)
		{
			funcdef.decl->Proto->cls = cls;
		}
		cls->Phase0();
		cls->Phase1();
		scope_mgr.template_trace_back_stack.pop_back();
		scope_mgr.RestoreClassTemplateEnv();
	}

	static void NoOpForTemplateInstance(ClassAST* dummy, vector<TemplateArgument>* v)
	{
		delete v;
	}

	static void NoOpForTemplateInstance(FunctionAST* dummy, vector<TemplateArgument>* v)
	{}

	template<typename T>
	T * Birdee::TemplateParameters<T>::GetOrCreate(vector<TemplateArgument>* v, T* source_template, SourcePos pos)
	{
		auto ins = instances.find(*v);
		if (ins != instances.end())
		{
			NoOpForTemplateInstance(source_template, v);
			return ins->second.get();
		}
		unique_ptr<T> replica_func = source_template->CopyNoTemplate();
		T* ret = replica_func.get();
		instances.insert(std::make_pair(reference_wrapper<const vector<TemplateArgument>>(*v), std::move(replica_func)));
		Phase1ForTemplateInstance(ret, source_template, *v, params, mod, pos);
		return ret;
	}

	template<typename T>
	void Birdee::TemplateParameters<T>::ValidateArguments(const vector<TemplateArgument>& args, SourcePos Pos)
	{
		DoTemplateValidateArguments(params,args, Pos);
	}
	void AddressOfExprAST::Phase1()
	{
		
		expr->Phase1();
		if (is_address_of)
			CompileAssert(expr->GetLValue(true), Pos, "The expression in addressof should be a LValue");			
		else
			CompileAssert(expr->resolved_type.isReference(), Pos, "The expression in pointerof should be a reference type");
		resolved_type.type = tok_pointer;
	}

	bool Birdee::IndexExprAST::isTemplateInstance()
	{
		if (!Expr)
			return true;
		auto func = GetFunctionFromExpression(Expr.get(), Pos);
		if (func && func->isTemplate())
		{
			return true;
		}
		return false;
	}

	void IndexExprAST::Phase1()
	{
		Expr->Phase1();
		if(isTemplateInstance())
		{
			vector<unique_ptr<ExprAST>> arg;
			arg.push_back(std::move(Index));
			instance = make_unique<FunctionTemplateInstanceExprAST>(std::move(Expr), std::move(arg),Pos);
			Expr = nullptr;
			instance->Phase1();
			resolved_type = instance->resolved_type;
			return;
		}
		CompileAssert(Expr->resolved_type.index_level > 0, Pos, "The indexed expression should be indexable");
		Index->Phase1();
		CompileAssert(Index->resolved_type.isInteger(), Pos, "The index should be an integer");
		resolved_type = Expr->resolved_type;
		resolved_type.index_level--;
	}

	void IdentifierExprAST::Phase1()
	{
		ImportTree* import_node;
		impl = scope_mgr.ResolveName(Name, Pos, import_node);
		if (!impl)
		{
			resolved_type.type = tok_package;
			resolved_type.import_node = import_node;
		}
		else
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
		if (!scope_mgr.basic_blocks.empty())			
			CompileAssert(scope_mgr.GetCurrentBasicBlock().find(name) == scope_mgr.GetCurrentBasicBlock().end(), Pos,
				"Variable name " + name + " has already been used in " + scope_mgr.GetCurrentBasicBlock()[name]->Pos.ToString());
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
		scope_mgr.GetCurrentBasicBlock()[name] = this;
	}

	void FunctionAST::Phase1()
	{
		if (isTemplate())
			return;
		auto prv_func = cur_func;
		cur_func = this;
		Phase0();
		Body.Phase1(Proto.get());
		cur_func = prv_func;
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

	ClassAST* GetArrayClass()
	{
		string name("genericarray");
		if (cu.is_corelib)
			return &(cu.classmap.find(name)->second.get());
		else
			return cu.imported_classmap.find(name)->second;
	}

	void CheckFunctionCallParameters(PrototypeAST* proto, std::vector<std::unique_ptr<ExprAST>>& Args, SourcePos Pos)
	{
		if (proto->resolved_args.size() != Args.size())
		{
			std::stringstream buf;
			buf << "The function requires " << proto->resolved_args.size() << " Arguments, but " << Args.size() << "are given";
			CompileAssert(false, Pos, buf.str());
		}

		int i = 0;
		for (auto& arg : Args)
		{
			arg->Phase1();
			SourcePos pos = arg->Pos;
			arg = FixTypeForAssignment(proto->resolved_args[i]->resolved_type, std::move(arg), pos);
			i++;
		}
	}

	void NewExprAST::Phase1()
	{
		resolved_type = ResolvedType(*ty, Pos);
		for (auto& expr : args)
		{
			expr->Phase1();
			if (resolved_type.index_level > 0)
			{
				CompileAssert(expr->resolved_type.isInteger(), expr->Pos, "Expected an integer for array size");
			}
		}
		if (resolved_type.index_level == 0)
		{
			if (!method.empty())
			{
				CompileAssert(resolved_type.type==tok_class, Pos, "new expression only supports class types");
				ClassAST* cls = resolved_type.class_ast;
				auto itr = cls->funcmap.find(method);
				CompileAssert(itr != cls->funcmap.end(), Pos, "Cannot resolve name "+ method);
				func = &cls->funcs[itr->second];
				CompileAssert(func->access==access_public, Pos, "Accessing a private method");
				CheckFunctionCallParameters(func->decl->Proto.get(), args, Pos);
			}
		}
	}

	void ClassAST::Phase1()
	{
		if (isTemplate())
			return;
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
		if (Obj->resolved_type.type == tok_package)
		{
			ImportTree* node = Obj->resolved_type.import_node;
			if (node->map.size() == 0)
			{
				auto ret1 = FindImportByName(node->mod->dimmap, member);
				if (ret1)
				{
					kind = member_imported_dim;
					import_dim = ret1;
					Obj = nullptr;
					resolved_type = ret1->resolved_type;
					return;
				}
				auto ret2 = FindImportByName(node->mod->funcmap, member);
				if (ret2)
				{
					kind = member_imported_function;
					import_func = ret2;
					Obj = nullptr;
					resolved_type = ret2->resolved_type;
					if (!resolved_type.isResolved()) //template function's resolved type is never resolved
						resolved_type.type = tok_func; //set resolved_type to avoid entering this function multiple times
					return;
				}
				throw CompileError(Pos.line,Pos.pos,"Cannot resolve name "+ member);
			}
			else
			{
				kind = member_package;
				resolved_type.type = tok_package;
				resolved_type.import_node = node->FindName(member);
				if(!resolved_type.import_node)
					throw CompileError(Pos.line, Pos.pos, "Cannot resolve name " + member);
				Obj = nullptr;
				return;
			}
			return;
		}
		if (Obj->resolved_type.index_level>0)
		{
			ClassAST* array_cls = preprocessing_state.array_cls;
			if (!array_cls)
			{
				array_cls = GetArrayClass();
				preprocessing_state.array_cls = array_cls;
			}
			auto func = array_cls->funcmap.find(member);
			if (func != array_cls->funcmap.end())
			{
				kind = member_function;
				this->func = &(array_cls->funcs[func->second]);
				if (this->func->access == access_private && !scope_mgr.IsCurrentClass(array_cls)) // if is private and we are not in the class
					throw CompileError(Pos.line, Pos.pos, "Accessing a private member outside of a class");
				resolved_type = this->func->decl->resolved_type;
				return;
			}
			throw CompileError(Pos.line, Pos.pos, "Cannot find member " + member);
			return;
		}
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
		throw CompileError(Pos.line, Pos.pos, "Cannot find member "+member);
	}

	void CallExprAST::Phase1()
	{
		Callee->Phase1();
		CompileAssert(Callee->resolved_type.type == tok_func, Pos, "The expression should be callable");
		auto func = GetFunctionFromExpression(Callee.get(),Pos);
		if (func)
			CompileAssert(!func->isTemplate(), Pos, "Cannot call a template");
		auto proto = Callee->resolved_type.proto_ast;
		CheckFunctionCallParameters(proto, Args, Pos);
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
		assert(cur_func->Proto->resolved_type.type != tok_error && "The prototype should be resolved first");
		Val = FixTypeForAssignment(cur_func->Proto->resolved_type, std::move(Val),Pos);
	}

	ClassAST* GetStringClass()
	{
		string name("string");
		if (cu.is_corelib)
			return &(cu.classmap.find(name)->second.get());
		else
			return cu.imported_classmap.find(name)->second;
	}



	void StringLiteralAST::Phase1()
	{
		//fix-me: use the system package name of string
		
		if(!preprocessing_state.string_cls)  
			preprocessing_state.string_cls = GetStringClass();
		resolved_type.type = tok_class;
		resolved_type.class_ast = preprocessing_state.string_cls;
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
			if (LHS->resolved_type.type == tok_class && LHS->resolved_type.index_level == 0) //if is class object, check for operator overload
			{
				static unordered_map<Token, string> operator_map = {
				{tok_add,"__add__"},
				{tok_minus,"__sub__"},
				{tok_mul,"__mul__"},
				{tok_div,"__div__"},
				{tok_mod,"__mod__"},
				{tok_cmp_equal,"__eq__"},
				{ tok_ge,"__ge__" },
				{ tok_le,"__le__" },
				{ tok_logic_and,"__logic_and__" },
				{ tok_logic_or,"__logic_or__" },
				{ tok_gt,"__gt__" },
				{ tok_lt,"__lt__" },
				{ tok_and,"__and__" },
				{ tok_or,"__or__" },
				{ tok_not,"__not__" },
				{ tok_xor,"__xor__" },
				};
				string& name = operator_map[Op];
				auto itr=LHS->resolved_type.class_ast->funcmap.find(name);
				CompileAssert(itr!= LHS->resolved_type.class_ast->funcmap.end(), Pos, string("Cannot find function") + name);
				func = LHS->resolved_type.class_ast->funcs[itr->second].decl.get();
				auto proto = func->resolved_type.proto_ast;
				vector<unique_ptr<ExprAST>> args;args.push_back(std::move(RHS) );
				CheckFunctionCallParameters(proto,args , Pos);
				RHS = std::move(args[0]);
				resolved_type = proto->resolved_type;
				return;
			}
			CompileAssert(LHS->resolved_type.isNumber() && RHS->resolved_type.isNumber(), Pos, "Currently only binary expressions of Numbers are supported");
			resolved_type.type = PromoteNumberExpression(LHS, RHS, isBooleanToken(Op), Pos);
		}

	}

	void ForBlockAST::Phase1()
	{
		scope_mgr.PushBasicBlock();
		init->Phase1();
		if (!isdim)
		{
			auto bin = dynamic_cast<BinaryExprAST*>(init.get());
			if (bin)
			{
				CompileAssert(bin->Op == tok_assign, Pos, "Expected an assignment expression after for");
				loop_var = bin->LHS.get();
			}
			else
			{
				CompileAssert(((ExprAST*)init.get())->GetLValue(true), Pos, "The loop variable in for block should be a LValue");
				loop_var = (ExprAST*)init.get();
			}
			CompileAssert(loop_var->resolved_type.isInteger(), Pos, "The loop variable should be an integer");
		}
		else
		{
			CompileAssert(((VariableSingleDefAST*)init.get())->resolved_type.isInteger(), Pos, "The loop variable should be an integer");
			loop_var = nullptr;
		}
		till->Phase1();
		for (auto&& node : block.body)
		{
			node->Phase1();
		}
		scope_mgr.PopBasicBlock();
	}
}