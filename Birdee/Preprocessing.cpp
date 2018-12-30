#include "BdAST.h"
#include "CompileError.h"
#include "SourcePos.h"
#include <cassert>
#include "CastAST.h"
#include <sstream>
#include "TemplateUtil.h"
#include <algorithm>
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


#ifdef BIRDEE_USE_DYN_LIB
#ifdef _WIN32
#include <Windows.h>
BD_CORE_API void* LoadBindingFunction(const char* name)
{
	auto hinst = LoadLibrary(L"BirdeeBinding.dll");
	assert(hinst);
	void* impl = GetProcAddress(hinst, name);
	assert(impl);
	return impl;
}

#define Birdee_RunAnnotationsOn_NAME "?Birdee_RunAnnotationsOn@@YAXAEAV?$vector@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$allocator@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@2@@std@@PEAVStatementAST@Birdee@@USourcePos@4@@Z"
#define Birdee_ScriptAST_Phase1_NAME "?Birdee_ScriptAST_Phase1@@YAXPEAVScriptAST@Birdee@@@Z"
#define Birdee_ScriptType_Resolve_NAME "?Birdee_ScriptType_Resolve@@YAXPEAVResolvedType@Birdee@@PEAVScriptType@2@USourcePos@2@@Z"

#else

#include <dlfcn.h>

BD_CORE_API void* LoadBindingFunction(const char* name)
{
	void *handle;
	char *error;
	handle = dlopen("libBirdeeBinding.so", RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}
	dlerror();
	auto impl=dlsym(handle, name);
	if ((error = dlerror()) != NULL) {
		fprintf(stderr, "%s\n", error);
		exit(1);
	}
	return impl;
}

#define Birdee_RunAnnotationsOn_NAME "_Z23Birdee_RunAnnotationsOnRSt6vectorINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESaIS5_EEPN6Birdee12StatementASTENS9_9SourcePosE"
#define Birdee_ScriptAST_Phase1_NAME "_Z23Birdee_ScriptAST_Phase1PN6Birdee9ScriptASTE"
#define Birdee_ScriptType_Resolve_NAME "_Z25Birdee_ScriptType_ResolvePN6Birdee12ResolvedTypeEPNS_10ScriptTypeENS_9SourcePosE"
#endif

static void Birdee_RunAnnotationsOn(std::vector<std::string>& anno, StatementAST* ths, SourcePos pos)
{

	typedef void(*PtrImpl)(std::vector<std::string>& anno, StatementAST* impl, SourcePos pos);
	static PtrImpl impl = nullptr;
	if (impl == nullptr)
	{
		impl = (PtrImpl)LoadBindingFunction(Birdee_RunAnnotationsOn_NAME);
	}
	impl(anno, ths, pos);
}
static void Birdee_ScriptAST_Phase1(ScriptAST* ths)
{
	typedef void(*PtrImpl)(ScriptAST* ths);
	static PtrImpl impl = nullptr;
	if (impl == nullptr)
	{
		impl = (PtrImpl)LoadBindingFunction(Birdee_ScriptAST_Phase1_NAME);
	}
	impl(ths);
}

static void Birdee_ScriptType_Resolve(ResolvedType* out, ScriptType* ths, SourcePos pos)
{
	typedef void(*PtrImpl)(ResolvedType* out,ScriptType* ths, SourcePos);
	static PtrImpl impl = nullptr;
	if (impl == nullptr)
	{
		impl = (PtrImpl)LoadBindingFunction(Birdee_ScriptType_Resolve_NAME);
	}
	impl(out, ths, pos);
}

#else
extern void Birdee_AnnotationStatementAST_Phase1(AnnotationStatementAST* ths);
extern void Birdee_ScriptAST_Phase1(ScriptAST* ths);
#endif


class ScopeManager
{
public:
	typedef unordered_map<reference_wrapper<const string>, VariableSingleDefAST*> BasicBlock;
	vector<ClassAST*> class_stack;
	//the top-level scope
	BasicBlock top_level_bb;
	//the sub-scopes of top-level code, they are not global
	vector<BasicBlock> top_level_scopes;

	//the scopes for nested functions
	//function_scopes.back() is the lowest level of nested function
	struct FunctionScope
	{
		vector<BasicBlock> scope;
		FunctionAST* func;
		FunctionScope(FunctionAST* f) :func(f) {}
	};
	vector<FunctionScope> function_scopes;
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

	VariableSingleDefAST* FindLocalVar(const string& name,const vector<BasicBlock>& basic_blocks)
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
		for (int i = 0; i< parameters.size(); i++)
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
		if (function_scopes.empty())
			//if we are in top-level code, push the scope in top-level's sub-scopes
			top_level_scopes.push_back(BasicBlock());
		else 
			//if we are in a function, push the scope in a function's scopes
			function_scopes.back().scope.push_back(BasicBlock());
	}
	void PopBasicBlock()
	{
		if (function_scopes.empty())
			//if we are in top-level code, pop the scope in top-level's sub-scopes
			top_level_scopes.pop_back();
		else
			//if we are in a function, pop the scope in a function's scopes
			function_scopes.back().scope.pop_back();
	}
	void PushClass(ClassAST* cls)
	{
		class_stack.push_back(cls);
	}
	void PopClass()
	{
		class_stack.pop_back();
	}

	bool isInTemplateOfOtherModuleAndDoImport()
	{
		bool ret = false;
		if (!template_stack.empty() && template_stack.back().imported_mod)
		{
			ret = true;
			template_stack.back().imported_mod->HandleImport();
		}
		if (!template_class_stack.empty() && template_class_stack.back().imported_mod)
		{
			ret = true;
			template_class_stack.back().imported_mod->HandleImport();
		}
		return ret;
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
					return var->second.get();
				auto& dimmap2 = vec.back().imported_mod->imported_dimmap;
				auto var2 = dimmap2.find(name);
				if (var2 != dimmap2.end())
					return var2->second;
			}
			return nullptr;
		};
		auto v = check_template_module(template_stack);
		if (v)
			return v;
		return check_template_module(template_class_stack); //will return null if not in template
	}

	unique_ptr<LocalVarExprAST> FindAndCreateCaptureVar(const string& name, SourcePos pos)
	{
		if (function_scopes.size() < 2)
			return nullptr;
		auto already_imported = function_scopes.back().func->GetImportedCapturedVariable(name);
		if (already_imported)
			return make_unique<LocalVarExprAST>(already_imported, pos);

		//iterate from the second last function scope
		for (auto itr = function_scopes.rbegin() + 1; itr !=function_scopes.rend(); itr++)
		{
			auto orig_var = FindLocalVar(name, itr->scope);
			if (!orig_var)
			{
				orig_var = itr->func->GetImportedCapturedVariable(name);
			}
			if (orig_var) //if variable is found
			{
				VariableSingleDefAST* v = orig_var;
				//import the variable in a chain back to the current function
				//(itr+1).base() converts itr from reverse iterator to normal iterator. They points to the same element
				for (auto itr2 = (itr+1).base(); itr2!=function_scopes.end()-1; itr2++)
				{
					//"v" is defined in function "*itr2"
					//first export v from *itr2
					auto capture_idx = itr2->func->CaptureVariable(v);
					unique_ptr<VariableSingleDefAST> var = unique_ptr_cast<VariableSingleDefAST>(v->Copy());
					
					//then create a variable to import the variable
					var->capture_import_type = v->capture_export_type;
					var->capture_import_idx = v->capture_export_idx;
					var->capture_export_idx = VariableSingleDefAST::CAPTURE_NONE;
					var->capture_export_idx = -1;

					//v is set to be the imported variable for the next iteration
					v = var.get();
					//in the child function, insert the imported variable
					(itr2 + 1)->func->imported_captured_var
						.insert(std::make_pair(std::reference_wrapper<const string>(var->name), std::move(var)));
				}
				return make_unique<LocalVarExprAST>(v, pos);
			}
		}
		return nullptr;
	}

	unique_ptr<ResolvedIdentifierExprAST> ResolveNameNoThrow(const string& name,SourcePos pos,ImportTree*& out_import)
	{
		auto bb = function_scopes.empty() ? FindLocalVar(name,top_level_scopes) : FindLocalVar(name, function_scopes.back().scope);
		if (bb)
		{
			return make_unique<LocalVarExprAST>(bb,pos);
		}
		auto template_arg = FindAndCopyTemplateArgument(name);
		if (template_arg)
			return std::move(template_arg);

		auto capture_var = FindAndCreateCaptureVar(name, pos);
		if (capture_var)
			return std::move(capture_var);

		auto cls_field = FindFieldInClass(name);
		if (cls_field)
		{
			auto ret = make_unique<ThisExprAST>(class_stack.back(), pos);
			ret->Phase1();
			return make_unique<MemberExprAST>(std::move(ret), cls_field, pos);
		}
		auto func_field = FindFuncInClass(name);
		if (func_field)
		{
			return make_unique<MemberExprAST>(make_unique<ThisExprAST>(class_stack.back(), pos), func_field, pos);
		}

		bool isInOtherModule = isInTemplateOfOtherModuleAndDoImport();
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
			if (!v.empty() && v.back().imported_mod)
			{
				auto& funcmap = v.back().imported_mod->funcmap;
				auto var = funcmap.find(name);
				if (var != funcmap.end())
					return make_unique<ResolvedFuncExprAST>(var->second.get(), pos);
				auto& funcmap2 = v.back().imported_mod->imported_funcmap;
				auto var2 = funcmap2.find(name);
				if (var2 != funcmap2.end())
					return make_unique<ResolvedFuncExprAST>(var2->second, pos);
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
		if (function_scopes.empty())
		{
			//if we are not in a function, first check if we are in a sub-scope of top-level
			if (!top_level_scopes.empty())
				return top_level_scopes.back();
			//..., or return the top-level scope
			return top_level_bb;
		}
		else
			return function_scopes.back().scope.back();
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

BD_CORE_API void PushClass(ClassAST* cls)
{
	return scope_mgr.PushClass(cls);
}

BD_CORE_API void PopClass()
{
	return scope_mgr.PopClass();
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

template <typename T, typename T2>
const T& GetItemByName(const unordered_map<T2, T>& M,
	const string& name)
{
	auto itr = M.find(name);
	if (itr == M.end())
		return nullptr;
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

template<typename Derived>
Derived* dyncast_resolve_anno(StatementAST* p)
{
	if (typeid(*p) == typeid(AnnotationStatementAST)) {
		return dyncast_resolve_anno<Derived>( static_cast<AnnotationStatementAST*>(p)->impl.get() );
	}
	if (typeid(*p) == typeid(Derived)) {
		return static_cast<Derived*>(p);
	}
	return nullptr;
}

static FunctionAST* GetFunctionFromExpression(ExprAST* expr,SourcePos Pos)
{
	FunctionAST* func = nullptr;
	IdentifierExprAST*  iden = dyncast_resolve_anno<IdentifierExprAST>(expr);
	if (iden)
	{
		auto resolvedfunc = dyncast_resolve_anno<ResolvedFuncExprAST>(iden->impl.get());
		if(resolvedfunc)
			func = resolvedfunc->def;
	}
	else
	{
		MemberExprAST*  mem = dyncast_resolve_anno<MemberExprAST>(expr);
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
	NullExprAST* expr = dyncast_resolve_anno<NullExprAST>(v);
	assert(expr);
	expr->resolved_type = target;
}

unique_ptr<ExprAST> FixTypeForAssignment(ResolvedType& target, unique_ptr<ExprAST>&& val,SourcePos pos)
{
	if (target == val->resolved_type)
	{
		return std::move(val);
	}
	if (target.type == tok_func && val->resolved_type.type == tok_func
		&& target.proto_ast->is_closure && !val->resolved_type.proto_ast->is_closure)
	{
		//if both types are functions and target is closure & source value is not closure
		if (target.proto_ast->IsSamePrototype(*val->resolved_type.proto_ast))
		{
			return make_unique<FunctionToClosureAST>(std::move(val));
		}
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
				if (isa<IdentifierExprAST>(ex->expr.get()))
				{
					IdentifierExprAST* iden = (IdentifierExprAST*)ex->expr.get();
					IdentifierType type(iden->Name);
					type.template_args = make_unique<vector<unique_ptr<ExprAST>>>(std::move(ex->raw_template_args));
					Type& dummy = type;
					this_template_args.push_back(TemplateArgument(ResolvedType(dummy, ex->Pos)));
				}
				else if (isa<MemberExprAST>(ex->expr.get()))
				{
					QualifiedIdentifierType type = QualifiedIdentifierType(((MemberExprAST*)ex->expr.get())->ToStringArray());
					type.template_args = make_unique<vector<unique_ptr<ExprAST>>>(std::move(ex->raw_template_args));
					Type& dummy = type;
					this_template_args.push_back(TemplateArgument(ResolvedType(dummy, ex->Pos)));
				}
				else if (isa<AnnotationStatementAST>(ex->expr.get()))
				{
					throw CompileError(ex->expr->Pos.line, ex->expr->Pos.pos, "The template argument cannot be annotated");
				}
				else
					assert(0 && "Not implemented");
			},
				[&this_template_args](IndexExprAST* ex) {
				if (isa<IdentifierExprAST>(ex->Expr.get()))
				{
					IdentifierType type(((IdentifierExprAST*)ex->Expr.get())->Name);
					type.template_args = make_unique<vector<unique_ptr<ExprAST>>>();
					type.template_args->push_back(std::move(ex->Index));
					Type& dummy = type;
					this_template_args.push_back(TemplateArgument(ResolvedType(dummy, ex->Pos)));
				}
				else if (isa<MemberExprAST>(ex->Expr.get()))
				{
					QualifiedIdentifierType type = QualifiedIdentifierType(((MemberExprAST*)ex->Expr.get())->ToStringArray());
					type.template_args = make_unique<vector<unique_ptr<ExprAST>>>();
					type.template_args->push_back(std::move(ex->Index));
					Type& dummy = type;
					this_template_args.push_back(TemplateArgument(ResolvedType(dummy, ex->Pos)));
				}
				else if (isa<AnnotationStatementAST>(ex->Expr.get()))
				{
					throw CompileError(ex->Expr->Pos.line, ex->Expr->Pos.pos, "The template argument cannot be annotated");
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
				[&template_arg]() {
				throw CompileError(template_arg->Pos.line, template_arg->Pos.pos,  "Invalid template argument expression type");
			}
			);
		}
	}

	void ResolvedType::ResolveType(Type& type, SourcePos pos)
	{
		if (type.type == tok_script)
		{
			Birdee_ScriptType_Resolve(this, static_cast<ScriptType*>(&type),pos);
			return;
		}
		if (type.type == tok_func)
		{
			auto t = static_cast<PrototypeType*>(&type);
			t->proto->Phase1(false);
			this->type = tok_func;
			this->index_level = type.index_level;
			this->proto_ast = t->proto.get();
			return;
		}

		if (type.type == tok_identifier)
		{
			IdentifierType* ty = dynamic_cast<IdentifierType*>(&type);
			assert(ty && "Type should be a IdentifierType");
			auto rtype=scope_mgr.FindTemplateType(ty->name);
			if (rtype.type != tok_error)
			{
				*this = rtype;
				return;
			}
			scope_mgr.isInTemplateOfOtherModuleAndDoImport();
			//if we are in a template and it is imported from another modules
			auto find_in_template_env = [&pos](ResolvedType& ths, const ScopeManager::TemplateEnv& env, const string& str)
			{
				//we are now acting as if we are compiling another module
				auto& functypemap = env.imported_mod->functypemap;
				auto fitr = functypemap.find(str);
				if (fitr != functypemap.end())
				{
					ths.type = tok_func;
					ths.proto_ast = fitr->second.get();
					return;
				}
				
				auto& functypemap2 = env.imported_mod->imported_functypemap;
				auto fitr2 = functypemap2.find(str);
				if (fitr2 != functypemap2.end())
				{
					ths.type = tok_func;
					ths.proto_ast = fitr2->second;
					return;
				}

				auto& classmap = env.imported_mod->classmap;
				auto itr = classmap.find(str);
				if (itr != classmap.end())
				{
					ths.type = tok_class;
					ths.class_ast = itr->second.get();
					return;
				}
				ths.type = tok_class;
				ths.class_ast = GetItemByName(env.imported_mod->imported_classmap, str, pos);
			};
			if (!scope_mgr.template_stack.empty() && scope_mgr.template_stack.back().imported_mod)
			{
				find_in_template_env(*this, scope_mgr.template_stack.back(),ty->name);
			}
			else if (!scope_mgr.template_class_stack.empty() && scope_mgr.template_class_stack.back().imported_mod)
			{
				find_in_template_env(*this, scope_mgr.template_class_stack.back(), ty->name);
			}
			else
			{

				auto& functypemap = cu.functypemap;
				auto fitr = functypemap.find(ty->name);
				if (fitr != functypemap.end())
				{
					this->type = tok_func;
					this->proto_ast = fitr->second.get();
				}
				else
				{
					auto& functypemap2 = cu.imported_functypemap;
					auto fitr2 = functypemap2.find(ty->name);
					if (fitr2 != functypemap2.end())
					{
						this->type = tok_func;
						this->proto_ast = fitr2->second;
					}
					else
					{
						this->type = tok_class;
						auto itr = cu.classmap.find(ty->name);
						if (itr == cu.classmap.end())
							this->class_ast = GetItemByName(cu.imported_classmap, ty->name, pos);
						else
							this->class_ast = &(itr->second.get());
					}
				}
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
			auto& functypemap2 = node->mod->functypemap;
			auto fitr2 = functypemap2.find(clsname);
			if (fitr2 != functypemap2.end())
			{
				this->type = tok_func;
				this->proto_ast = fitr2->second.get();
			}
			else
			{
				auto itr = node->mod->classmap.find(clsname);
				if (itr == node->mod->classmap.end())
					throw CompileError(pos.line, pos.pos, "Cannot find type name " + clsname + " in module " + GetModuleNameByArray(ty->name));
				this->type = tok_class;
				this->class_ast = itr->second.get();
			}
		}
		if (this->type==tok_class &&(type.type == tok_identifier || type.type == tok_package))
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
		if(this->type==tok_class)
			CompileAssert(!this->class_ast->isTemplate(), pos, string("Cannot use template as a class instance: ") + this->class_ast->GetUniqueName());

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
		for (auto& node : functypemap)
		{
			node.second->Phase1(false);
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
		if (type == tok_null)
			return "null_t";
		if (type == tok_func)
		{
			string ret;
			if (proto_ast->is_closure)
				ret = "closure";
			else
				ret = "functype";
			ret += " (";
			for (auto& arg : proto_ast->resolved_args)
			{
				ret += arg->resolved_type.GetString();
				ret += ", ";
			}
			if (ret.back() != '(')
			{
				ret.pop_back();
				ret.pop_back(); //delete the last ", "
			}
			ret += ')';
			if (proto_ast->resolved_type.type != tok_void)
			{
				ret += " as ";
				ret += proto_ast->resolved_type.GetString();
			}
			return ret;
		}
		if (type == tok_void)
			return "void";

		std::stringstream buf;
		if (type == tok_class)
			buf << GetClassASTName(class_ast);
		else if (isTypeToken(type))
			buf << GetTokenString(type);
		else
			return "(Error type)";
		for (int i = 0; i < index_level; i++)
			buf << "[]";
		return buf.str();
	}

	bool PrototypeAST::IsSamePrototype(const PrototypeAST& other) const
	{
		auto& ths = *this;
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

	std::size_t PrototypeAST::rawhash() const
	{
		const PrototypeAST* proto = this;
		size_t v = proto->resolved_type.rawhash() << 3; //return type
		v ^= (uintptr_t)proto->cls; //belonging class
		v ^= proto->is_closure;
		int offset = 6;
		for (auto& arg : proto->resolved_args) //argument types
		{
			v ^= arg->resolved_type.rawhash() << offset;
			offset = (offset + 3) % 32;
		}
		return v;
	}

	bool operator==(const PrototypeAST& ths, const PrototypeAST& other)
	{
		if (ths.is_closure != other.is_closure) //if is_closure field is not the same
			return false;
		if (ths.cls != other.cls)
			return false;
		return ths.IsSamePrototype(other);
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

	void Birdee::ScriptAST::Phase1()
	{
		Birdee_ScriptAST_Phase1(this);
	}

	llvm::Value* Birdee::AnnotationStatementAST::GetLValue(bool checkHas)
	{
		CompileAssert(is_expr, Pos, "Cannot get LValue of an non-expression");
		return GetLValueNoCheckExpr(checkHas);
	}


	void Birdee::AnnotationStatementAST::Phase1()
	{
		if (isa<ClassAST>(impl.get()))
		{
			ClassAST* cls = static_cast<ClassAST*>(impl.get());
			if (cls->isTemplate())
			{
				for (auto& inst : cls->template_param->instances)
				{
					Birdee_RunAnnotationsOn(anno, inst.second.get(), inst.second->Pos);
				}
				cls->template_param->annotation = this;
				return;
			}
			//if is not a template, fall through to the following code
		}
		else if(isa<FunctionAST>(impl.get()))
		{
			FunctionAST* func = static_cast<FunctionAST*>(impl.get());
			if (func->isTemplate())
			{
				for (auto& inst : func->template_param->instances)
				{
					Birdee_RunAnnotationsOn(anno, inst.second.get(), inst.second->Pos);
				}
				func->template_param->annotation = this;
				return;
			}
			//if is not a template, fall through to the following code
		}
		impl->Phase1();
		Birdee_RunAnnotationsOn(anno,this,impl->Pos);
		if (is_expr)
		{
			resolved_type = static_cast<ExprAST*>(impl.get())->resolved_type;
		}
	}

	VariableSingleDefAST * FunctionAST::GetImportedCapturedVariable(const string & name)
	{
		auto itr=imported_captured_var.find(name);
		if (itr != imported_captured_var.end())
		{
			return itr->second.get();
		}
		return nullptr;
	}
	size_t FunctionAST::CaptureVariable(VariableSingleDefAST * var)
	{
		auto itr=std::find(captured_var.begin(),captured_var.end(),var);
		if (itr != captured_var.end())
			return itr - captured_var.begin();
		if (var->capture_import_type == VariableSingleDefAST::CAPTURE_NONE)
			var->capture_export_type = VariableSingleDefAST::CAPTURE_VAL;
		else // if CAPTURE_VAL or CAPTURE_REF
			var->capture_export_type = VariableSingleDefAST::CAPTURE_REF;
		captured_var.push_back(var);
		var->capture_export_idx = captured_var.size() - 1;
		return var->capture_export_idx;
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
			throw CompileError(expr->Pos.line, expr->Pos.pos, "The expression is neither NumberExprAST nor StringLiteralAST");
		}
		else if(s1)
		{
			if (s2)
				return s1->Val < s2->Val;
			if (n2)
				return false; 
			throw CompileError(expr->Pos.line, expr->Pos.pos, "The expression is neither NumberExprAST nor StringLiteralAST");
		}
		throw CompileError(expr->Pos.line, expr->Pos.pos, "The expression is neither NumberExprAST nor StringLiteralAST");
		return false;
	}

	string int2str(const int v)
	{
		std::stringstream stream;
		stream << v;
		return stream.str();   
	}

	static void DoTemplateValidateArguments(bool is_vararg, const vector<TemplateParameter>& params, const vector<TemplateArgument>& args, SourcePos Pos)
	{
		if (!is_vararg)
		{
			CompileAssert(params.size() == args.size(), Pos,
				string("The template requires ") + int2str(params.size()) + " arguments, but " + int2str(args.size()) + " are given");
		}
		else
		{
			CompileAssert(params.size() <= args.size(), Pos,
				string("The template requires at least ") + int2str(params.size()) + " arguments, but " + int2str(args.size()) + " are given");
		}
		
		for (int i = 0; i < params.size(); i++)
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
			if (!isa<MemberExprAST>(cur))
			{
				CompileAssert(!isa<AnnotationStatementAST>(cur), cur->Pos, "Template arugments cannot be annotated");
				CompileAssert(isa<IdentifierExprAST>(cur), cur->Pos, "Expected an identifier for template");
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
					CompileAssert(number , arg.expr->Pos, "Template arg should be string or number constant");
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
		auto basic_blocks_backup = std::move(scope_mgr.function_scopes);
		scope_mgr.function_scopes = vector <ScopeManager::FunctionScope>();
		func->isTemplateInstance = true;
		func->Phase0();
		func->Phase1();
		scope_mgr.RestoreTemplateEnv();
		scope_mgr.template_trace_back_stack.pop_back();
		scope_mgr.function_scopes = std::move(basic_blocks_backup);
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
		if (annotation)
		{
			Birdee_RunAnnotationsOn(annotation->anno,ret,ret->Pos);
		}
		return ret;
	}

	template<typename T>
	void Birdee::TemplateParameters<T>::ValidateArguments(const vector<TemplateArgument>& args, SourcePos Pos)
	{
		DoTemplateValidateArguments(is_vararg, params, args, Pos);
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
		proto->Phase1(true);
		for (auto&& node : body)
		{
			node->Phase1();
		}
		scope_mgr.PopBasicBlock();
	}

	void PrototypeAST::Phase0()
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
		resolved_type = ResolvedType(*RetType, pos);
	}

	void PrototypeAST::Phase1(bool register_in_basic_block)
	{
		Phase0();
		for (auto&& dim : resolved_args)
		{
			dim->Phase1InFunctionType(register_in_basic_block);
		}
	}


	void VariableSingleDefAST::Phase1InFunctionType(bool register_in_basic_block)
	{
		if (register_in_basic_block)
			CompileAssert(scope_mgr.GetCurrentBasicBlock().find(name) == scope_mgr.GetCurrentBasicBlock().end(), Pos,
				"Variable name " + name + " has already been used in " + scope_mgr.GetCurrentBasicBlock()[name]->Pos.ToString());
		Phase0();
		CompileAssert(resolved_type.type != tok_auto, Pos, "Must specify a type for the parameter");
		CompileAssert(val.get()==nullptr, Pos, "Parameters cannot have initializers");
		if(register_in_basic_block)
			scope_mgr.GetCurrentBasicBlock()[name] = this;
	}

	void VariableSingleDefAST::Phase1()
	{
		CompileAssert(scope_mgr.GetCurrentBasicBlock().find(name) == scope_mgr.GetCurrentBasicBlock().end(), Pos,
				"Variable name " + name + " has already been used in " + scope_mgr.GetCurrentBasicBlock()[name]->Pos.ToString());
		Phase0();
		if (resolved_type.type == tok_auto)
		{
			CompileAssert(val.get(), Pos, "Variable definition without a type must have an initializer");
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
		if (scope_mgr.function_scopes.size() > 0)
			parent = scope_mgr.function_scopes.back().func;
		scope_mgr.function_scopes.emplace_back(ScopeManager::FunctionScope(this));
		auto prv_func = cur_func;
		cur_func = this;
		Phase0();
		if (scope_mgr.function_scopes.size() > 1) //if we are in a lambda func
			Proto->cls = nullptr;   //"this" pointer is included in the captured lambda
		Body.Phase1(Proto.get());

		if (captured_parent_this) //if "this" is used, capture "this" in all parent functions
		{
			for (int i = 0; i <= scope_mgr.function_scopes.size() - 2; i++)
				scope_mgr.function_scopes[i].func->capture_this = true;
		}
		if (!imported_captured_var.empty() || captured_parent_this) //if captured any parent var, set the function type: is_closure=true
			Proto->is_closure = true;
		cur_func = prv_func;
		scope_mgr.function_scopes.pop_back();
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
			buf << "The function requires " << proto->resolved_args.size() << " Arguments, but " << Args.size() << " are given";
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
			CompileAssert(resolved_type.type == tok_class, Pos, "new expression only supports class types");
			ClassAST* cls = resolved_type.class_ast;
			if (!method.empty())
			{
				auto itr = cls->funcmap.find(method);
				CompileAssert(itr != cls->funcmap.end(), Pos, "Cannot resolve name "+ method);
				func = &cls->funcs[itr->second];
				CompileAssert(func->access==access_public, Pos, "Accessing a private method");
				CheckFunctionCallParameters(func->decl->Proto.get(), args, Pos);
			} else { // no specifically calling __init__, complier tries to find one
				string init_method = "__init__";
				auto itr = cls->funcmap.find(init_method);
				if (itr != cls->funcmap.end()) // check if available __init__() exists
				{ 
					auto tfunc = &cls->funcs[itr->second];
					CompileAssert(tfunc->access == access_public, Pos, string("__init__ function of class ")
						+ resolved_type.class_ast->GetUniqueName() + " is defined, but is not public");
					CompileAssert(tfunc->decl->Proto.get()->resolved_args.size() == 0, Pos, string("__init__ function of class ")
						+ resolved_type.class_ast->GetUniqueName() + " is defined, but has more than zero arguments"); 
					func = tfunc;
				}
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
			if (funcdef.decl->GetName() == "__del__")
			{
				CompileAssert(funcdef.decl->Proto->resolved_args.size() == 0, funcdef.decl->Pos, "__del__ destructor must have no arguments");
				auto& type = funcdef.decl->Proto->resolved_type;
				CompileAssert(type.type==tok_void && type.index_level==0, funcdef.decl->Pos, "__del__ destructor must not have return values");
				CompileAssert(funcdef.access==access_public, funcdef.decl->Pos, "__del__ destructor must be public");
			}
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
		auto func = GetFunctionFromExpression(Callee.get(),Pos);
		// if (func)
		// 	CompileAssert(!func->isTemplate(), Pos, "Cannot call a template");
		if (func && func->isTemplate()) {
			// tries to derive template arguments from Args
			auto args = func->Proto->Args.get();
			vector<VariableSingleDefAST*> singleArgs;
			if (isa<VariableSingleDefAST>(args)) {
				auto singleArg = static_cast<VariableSingleDefAST*>(args);
				singleArgs.push_back(singleArg);
			} else {
				assert(isa<VariableMultiDefAST>(args));
				auto multiArg = static_cast<VariableMultiDefAST*>(args);
				for (auto & singleArg : multiArg->lst) {
					singleArgs.push_back(singleArg.get());
				}
			}

			if (singleArgs.size() != Args.size()) {
				std::stringstream buf;
				buf << "The function requires " << singleArgs.size() << " Arguments, but " << Args.size() << "are given";
				CompileAssert(false, Pos, buf.str());
			}

			unordered_map<string, ResolvedType> name2type;
			for (int i = 0; i < singleArgs.size(); i++) {
				Args[i]->Phase1();
				if (singleArgs[i]->type->type == tok_identifier) {
					auto identifierType = static_cast<IdentifierType*>(singleArgs[i]->type.get());
					auto it = name2type.find(identifierType->name);
					
					if (it == name2type.end())
						name2type[identifierType->name] = Args[i]->resolved_type;
					else if (!(it->second == Args[i]->resolved_type))
						CompileAssert(false, Pos, string("Cannot derive template parameter type ")+ identifierType->name 
							+ " from given arguments: conflicting argument types - " + it->second.GetString() + " and " + Args[i]->resolved_type.GetString());
				}
			}

			auto & params = func->template_param->params;
			// construct FunctionTemplateInstanceAST to replace Callee
			vector<TemplateArgument> template_args;
			for (auto & param : params) {
				CompileAssert(param.isTypeParameter(), Pos, "Cannot derive the value template parameter: " + param.name);
				auto itr = name2type.find(param.name);
				CompileAssert(itr != name2type.end(), Pos, "Cannot derive the template parameter: " + param.name);
				template_args.emplace_back(TemplateArgument(itr->second));
			}
			func->template_param->ValidateArguments(template_args, Pos);
			auto instance = func->template_param->GetOrCreate(&template_args, func, Pos);
			if (auto memb = dyncast_resolve_anno<MemberExprAST>(Callee.get()))
			{
				memb->kind = MemberExprAST::member_imported_function;
				memb->import_func = instance;
				memb->resolved_type = instance->resolved_type;
			}
			else
			{
				Callee = make_unique<ResolvedFuncExprAST>(instance, Callee->Pos);
				Callee->Phase1();
			}
			// do phase1 again
		}
		CompileAssert(Callee->resolved_type.type == tok_func, Pos, "The expression should be callable");
		// TODO: do not arg->Phase1 again
		auto proto = Callee->resolved_type.proto_ast;
		CheckFunctionCallParameters(proto, Args, Pos);
		resolved_type = proto->resolved_type;
	}

	void ThisExprAST::Phase1()
	{
		CompileAssert(scope_mgr.class_stack.size() > 0, Pos, "Cannot reference \"this\" outside of a class");
		if (scope_mgr.function_scopes.size() > 1) //if we are in a lambda func
		{
			scope_mgr.function_scopes.back().func->captured_parent_this = (llvm::Value*)1;
		}
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
		auto& LHS = this->LHS;//just for lambda capture
		auto& RHS = this->RHS;
		auto& resolved_type = this->resolved_type;
		auto& Pos = this->Pos;
		auto& func = this->func;

		LHS->Phase1();
		RHS->Phase1();
		if (Op == tok_assign)
		{
			if (IdentifierExprAST* idexpr = dyncast_resolve_anno<IdentifierExprAST>(LHS.get()))
				CompileAssert(idexpr->impl->isMutable(), Pos, "Cannot assign to an immutable value");
			else if (MemberExprAST* memexpr = dyncast_resolve_anno<MemberExprAST>(LHS.get()))
				CompileAssert(memexpr->isMutable(), Pos, "Cannot assign to an immutable value");
			else if (dyncast_resolve_anno<IndexExprAST>(LHS.get()))
			{}
			else
				throw CompileError(Pos.line, Pos.pos, "The left vaule of the assignment is not an variable");
			RHS = FixTypeForAssignment(LHS->resolved_type, std::move(RHS), Pos);
			resolved_type.type = tok_void;
			return;
		}
		auto gen_call_to_operator_func = [&LHS,&RHS,&resolved_type,&Pos,&func](const string name) {
			auto itr = LHS->resolved_type.class_ast->funcmap.find(name);
			CompileAssert(itr != LHS->resolved_type.class_ast->funcmap.end(), Pos, 
				string("Cannot find function ") + name + " in class " + LHS->resolved_type.class_ast->GetUniqueName());
			func = LHS->resolved_type.class_ast->funcs[itr->second].decl.get();
			auto proto = func->resolved_type.proto_ast;
			vector<unique_ptr<ExprAST>> args; args.push_back(std::move(RHS));
			CheckFunctionCallParameters(proto, args, Pos);
			RHS = std::move(args[0]);
			resolved_type = proto->resolved_type;
		};
		if (Op == tok_equal || Op == tok_ne)
		{
			if (LHS->resolved_type.type == tok_class && LHS->resolved_type.index_level == 0) //if is class object, check for operator overload
				gen_call_to_operator_func(Op == tok_equal ? "__eq__": "__ne__");
			else if (LHS->resolved_type == RHS->resolved_type)
				resolved_type.type = tok_boolean;
			else
				resolved_type.type=PromoteNumberExpression(LHS, RHS, true, Pos);
		}
		else if (Op == tok_cmp_equal || Op == tok_cmp_ne)
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
				resolved_type.type = PromoteNumberExpression(LHS, RHS, true, Pos);
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
				gen_call_to_operator_func(name);
				return;
			}
			if (LHS->resolved_type.index_level == 0 && LHS->resolved_type.type == tok_boolean
				&& RHS->resolved_type.index_level == 0 && RHS->resolved_type.type == tok_boolean) //boolean
			{
				if(isLogicToken(Op))
				{
					resolved_type = LHS->resolved_type;
					return;
				}
				CompileAssert(false, Pos, "Unsupported operator on boolean values");
			}
			CompileAssert(LHS->resolved_type.isNumber() && RHS->resolved_type.isNumber(), Pos, "Currently only binary expressions of Numbers are supported");
			if (isLogicToken(Op))
			{
				CompileAssert(LHS->resolved_type.isInteger() && RHS->resolved_type.isInteger(), Pos, "Logical operators can only be applied on integers or booleans");
				CompileAssert(Op != tok_logic_and && Op != tok_logic_or, Pos, "Shortcut logical operators can only be applied on booleans");
			}
			resolved_type.type = PromoteNumberExpression(LHS, RHS, false, Pos);
		}

	}

	void ForBlockAST::Phase1()
	{
		scope_mgr.PushBasicBlock();
		init->Phase1();
		if (!isdim)
		{
			auto bin = dyncast_resolve_anno<BinaryExprAST>(init.get());
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

	void Birdee::WhileBlockAST::Phase1()
	{
		cond->Phase1();
		CompileAssert(cond->resolved_type.type == tok_boolean && cond->resolved_type.index_level == 0, Pos, "Expecting a boolean expression in while block");
		block.Phase1();
	}

	llvm::Value * Birdee::ScriptAST::GetLValue(bool checkHas)
	{
		CompileAssert(instance_of<ExprAST>(stmt.get()), Pos, "Getting LValue from statement is illegal");
		return static_cast<ExprAST*>(stmt.get())->GetLValue(checkHas);
	}
}