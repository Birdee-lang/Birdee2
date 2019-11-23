#include "BdAST.h"
#include "CompileError.h"
#include "SourcePos.h"
#include <cassert>
#include "CastAST.h"
#include <sstream>
#include "TemplateUtil.h"
#include <algorithm>
#include <unordered_set>
using std::unordered_map;
using std::string;
using std::reference_wrapper;
using std::unordered_set;

using namespace Birdee;

//fix-me: show template stack on error

#define type_is_class(_rty) (_rty.type==tok_class && !_rty.class_ast->is_struct)

template <typename T>
T* FindImportByName(const unordered_map<reference_wrapper<const string>, T*>& M,
	const string& name)
{
	auto itr = M.find(name);
	if (itr == M.end())
		return nullptr;
	return (itr->second);
}

namespace Birdee
{
	class TemplateArgumentNumberError : public CompileError
	{
	public:
		FunctionAST* src_template;
		std::shared_ptr<vector<TemplateArgument>> args;

		TemplateArgumentNumberError(SourcePos pos, const std::string& _msg,
			FunctionAST* src_template, unique_ptr<vector<TemplateArgument>>&& args) :
			src_template(src_template), args(std::move(args)), CompileError(pos,_msg)
		{}
	};
	extern bool IsResolvedTypeClass(const ResolvedType& r);
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

#define Birdee_RunAnnotationsOn_NAME "?Birdee_RunAnnotationsOn@@YAXAEAV?$vector@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$allocator@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@2@@std@@PEAVStatementAST@Birdee@@USourcePos@4@PEAX@Z"
#define Birdee_ScriptAST_Phase1_NAME "?Birdee_ScriptAST_Phase1@@YAXPEAVScriptAST@Birdee@@PEAX1@Z"
#define Birdee_ScriptType_Resolve_NAME "?Birdee_ScriptType_Resolve@@YAXPEAVResolvedType@Birdee@@PEAVScriptType@2@USourcePos@2@PEAX3@Z"
#define BirdeeCopyPyScope_NAME "?BirdeeCopyPyScope@@YAPEAXPEAX@Z"
#define BirdeeDerefObj_NAME "?BirdeeDerefObj@@YAXPEAX@Z"
#define BirdeeGetOrigScope_NAME "?BirdeeGetOrigScope@@YAPEAXXZ"
#define Birdee_Register_Module_NAME "?Birdee_Register_Module@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PEAX@Z"
#define Birdee_RunScriptForString_NAME "?Birdee_RunScriptForString@@YA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV12@AEBUSourcePos@Birdee@@@Z"
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

#define Birdee_RunAnnotationsOn_NAME "_Z23Birdee_RunAnnotationsOnRSt6vectorINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESaIS5_EEPN6Birdee12StatementASTENS9_9SourcePosEPv"
#define Birdee_ScriptAST_Phase1_NAME "_Z23Birdee_ScriptAST_Phase1PN6Birdee9ScriptASTEPvS2_"
#define Birdee_ScriptType_Resolve_NAME "_Z25Birdee_ScriptType_ResolvePN6Birdee12ResolvedTypeEPNS_10ScriptTypeENS_9SourcePosEPvS5_"
#define BirdeeCopyPyScope_NAME "_Z17BirdeeCopyPyScopePv"
#define BirdeeDerefObj_NAME "_Z14BirdeeDerefObjPv"
#define BirdeeGetOrigScope_NAME "_Z18BirdeeGetOrigScopev"
#define Birdee_Register_Module_NAME "_Z22Birdee_Register_ModuleRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEPv"
#define Birdee_RunScriptForString_NAME "_Z25Birdee_RunScriptForStringRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERKN6Birdee9SourcePosE"
#endif

static void Birdee_RunAnnotationsOn(std::vector<std::string>& anno, StatementAST* ths, SourcePos pos, void* globalscope)
{

	typedef void(*PtrImpl)(std::vector<std::string>& anno, StatementAST* impl, SourcePos pos, void* globalscope);
	static PtrImpl impl = nullptr;
	if (impl == nullptr)
	{
		impl = (PtrImpl)LoadBindingFunction(Birdee_RunAnnotationsOn_NAME);
	}
	impl(anno, ths, pos, globalscope);
}
static void Birdee_ScriptAST_Phase1(ScriptAST* ths, void* globalscope, void* localscope)
{
	typedef void(*PtrImpl)(ScriptAST* ths, void* globalscope, void* scope);
	static PtrImpl impl = nullptr;
	if (impl == nullptr)
	{
		impl = (PtrImpl)LoadBindingFunction(Birdee_ScriptAST_Phase1_NAME);
	}
	impl(ths, globalscope, localscope);
}

static void Birdee_ScriptType_Resolve(ResolvedType* out, ScriptType* ths, SourcePos pos, void* globalscope, void* localscope)
{
	typedef void(*PtrImpl)(ResolvedType* out,ScriptType* ths, SourcePos, void* globalscope, void* localscope);
	static PtrImpl impl = nullptr;
	if (impl == nullptr)
	{
		impl = (PtrImpl)LoadBindingFunction(Birdee_ScriptType_Resolve_NAME);
	}
	impl(out, ths, pos, globalscope, localscope);
}

static void* BirdeeCopyPyScope(void* src)
{
	typedef void*(*PtrImpl)(void* src);
	static PtrImpl impl = nullptr;
	if (impl == nullptr)
	{
		impl = (PtrImpl)LoadBindingFunction(BirdeeCopyPyScope_NAME);
	}
	return impl(src);
}

static void BirdeeDerefObj(void* obj)
{
	typedef void*(*PtrImpl)(void* obj);
	static PtrImpl impl = nullptr;
	if (impl == nullptr)
	{
		impl = (PtrImpl)LoadBindingFunction(BirdeeDerefObj_NAME);
	}
	impl(obj);
}

static void* BirdeeGetOrigScope()
{
	static void* orig_scope = nullptr;
	if (!orig_scope)
	{

		typedef void*(*PtrImpl)();
		static PtrImpl impl = nullptr;
		if (impl == nullptr)
		{
			impl = (PtrImpl)LoadBindingFunction(BirdeeGetOrigScope_NAME);
		}
		orig_scope = impl();
	}
	return orig_scope;
}
void Birdee_Register_Module(const string& name, void* globals)
{
	typedef void*(*PtrImpl)(const string& name, void* globals);
	static PtrImpl impl = nullptr;
	if (impl == nullptr)
	{
		impl = (PtrImpl)LoadBindingFunction(Birdee_Register_Module_NAME);
	}
	impl(name, globals);
}


string Birdee_RunScriptForString(const string& str, const SourcePos& pos)
{
	typedef string(*PtrImpl)(const string& name, const SourcePos& pos);
	static PtrImpl impl = nullptr;
	if (impl == nullptr)
	{
		impl = (PtrImpl)LoadBindingFunction(Birdee_RunScriptForString_NAME);
	}
	return std::move(impl(str, pos));
}
#else
extern void Birdee_RunAnnotationsOn(std::vector<std::string>& anno, StatementAST* ths, SourcePos pos, void* globalscope);
extern void Birdee_ScriptAST_Phase1(ScriptAST* ths, void* globalscope, void* localscope);
extern void Birdee_ScriptType_Resolve(ResolvedType* out, ScriptType* ths, SourcePos pos, void* globalscope, void* localscope);
extern void* BirdeeCopyPyScope(void* src);
extern void BirdeeDerefObj(void* obj);
extern void* BirdeeGetOrigScope();
extern void Birdee_Register_Module(const string& name, void* globals);
#endif

#define CompileAssert(a, p ,msg)\
do\
{\
	if (!(a))\
	{\
		throw CompileError(p, msg);\
	}\
}while(0)\

template<typename T>
inline T* FindModuleGlobal(unordered_map<string, std::pair<unique_ptr<T>, bool>>& v,const string& name, bool can_read_private)
{
	auto itr = v.find(name);
	if (itr != v.end() && (can_read_private || itr->second.second) )
		return itr->second.first.get();
	return nullptr;
}

class ScopeManager
{
public:
	typedef unordered_map<reference_wrapper<const string>, VariableSingleDefAST*> BasicBlock;
	vector<ClassAST*> class_stack;
	//the top-level scope
	BasicBlock top_level_bb;
	//the sub-scopes of top-level code, they are not global
	vector<BasicBlock> top_level_scopes;
	//extension functions need an extra PrototypeAST in member ast, to remove the first parameter
	unordered_map<FunctionAST*, PrototypeAST*> extension_proto_cache;

	//the scope dicts for python. May be null it script is not current executed
	//One PyScope object represents a Birdee module. scope_dicts field represents the nested scopes of the module.
	//mod->py_scope is the "globals" scope of the module
	struct PyScope
	{
		ImportedModule* mod; // null for main module
		vector<Birdee::PyHandle> scope_dicts;
		PyScope(ImportedModule* mod) : mod(mod) {}
	};
	vector<PyScope> py_scope_dicts; 

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
		bool isEmpty;
		ImportedModule* imported_mod;
		TemplateEnv():pos(0,0,0), isClass(false),imported_mod(nullptr), isEmpty(true){};
		TemplateEnv(SourcePos pos, bool isClass, ImportedModule* imported_mod): 
			pos(pos), isClass(isClass), imported_mod(imported_mod), isEmpty(false){}
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

	ScopeManager()
	{
		//initially push a PyScope for the current main module
		py_scope_dicts.emplace_back(PyScope(nullptr));
	}

	inline bool IsCurrentClass(ClassAST* cls)
	{
		return class_stack.size() > 0 && class_stack.back() == cls;
	}

	PrototypeAST* GetExtensionFunctionPrototype(FunctionAST* f)
	{
		auto itr = extension_proto_cache.find(f);
		if (itr != extension_proto_cache.end())
			return itr->second;
		auto ptr = f->Proto->Copy();
		auto& args = f->Proto->resolved_args;
		assert(args.size() && IsResolvedTypeClass(args.front()->resolved_type));
		ptr->cls = args.front()->resolved_type.class_ast;
		ptr->is_closure = true;
		ptr->resolved_args.erase(ptr->resolved_args.begin()); // remove the first one
		auto ret = ptr.get();
		cu.generated_functy.emplace_back(std::move(ptr));
		return ret;
	}

	FieldDef* FindFieldInClass(const string& name)
	{
		if (class_stack.size() > 0)
		{
			ClassAST* cls = class_stack.back();
			ClassAST* cur_cls = cls;
			while (cur_cls)
			{
				auto cls_field = cur_cls->fieldmap.find(name);
				if (cls_field != cur_cls->fieldmap.end())
				{
					return &(cur_cls->fields[cls_field->second]);
				}
				cur_cls = cur_cls->parent_class;
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
			ClassAST* cur_cls = cls;
			while (cur_cls)
			{
				auto func_field = cur_cls->funcmap.find(name);
				if (func_field != cur_cls->funcmap.end())
				{
					return &(cur_cls->funcs[func_field->second]);
				}
				cur_cls = cur_cls->parent_class;
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
		py_scope_dicts.push_back(PyScope(mod));
		TemplateEnv env = CreateTemplateEnv(template_args, parameters,false, mod, pos);
		template_stack.push_back(std::move(env));
	}

	void RestoreTemplateEnv()
	{
		py_scope_dicts.pop_back();
		template_stack.pop_back();
	}

	void SetEmptyTemplateEnv(PyScope&& scope)
	{
		py_scope_dicts.push_back(std::move(scope));
		template_stack.push_back(TemplateEnv());
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

	bool IsInTopLevel() 
	{
		return function_scopes.empty();
	}

	void PushBasicBlock()
	{
		py_scope_dicts.back().scope_dicts.push_back(nullptr);
		if (function_scopes.empty())
			//if we are in top-level code, push the scope in top-level's sub-scopes
			top_level_scopes.push_back(BasicBlock());
		else 
			//if we are in a function, push the scope in a function's scopes
			function_scopes.back().scope.push_back(BasicBlock());
	}
	void PopBasicBlock()
	{
		py_scope_dicts.back().scope_dicts.pop_back();
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
				if (auto v = FindModuleGlobal(dimmap, name, true))
					return v;
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
					unique_ptr<VariableSingleDefAST> var = v->CopyNoInitializer();
					
					//then create a variable to import the variable
					var->capture_import_type = v->capture_export_type;
					var->capture_import_idx = v->capture_export_idx;
					var->capture_export_type = VariableSingleDefAST::CAPTURE_NONE;
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
			auto ret = make_unique<MemberExprAST>(make_unique<ThisExprAST>(class_stack.back(), pos), name);
			ret->Phase1();
			return ret;
		}
		auto func_field = FindFuncInClass(name);
		if (func_field)
		{
			auto ret = make_unique<MemberExprAST>(make_unique<ThisExprAST>(class_stack.back(), pos), name);
			ret->Phase1();
			return ret;
			// return make_unique<MemberExprAST>(make_unique<ThisExprAST>(class_stack.back(), pos), func_field, pos);
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
				if(auto var = FindModuleGlobal(funcmap, name, true))
					return make_unique<ResolvedFuncExprAST>(var, pos);
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
				return make_unique<ResolvedFuncExprAST>(func->second.first, pos);
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
			throw CompileError(pos, "Cannot resolve name: " + name);
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
	PyHandle main_global_scope;
	FunctionAST* _cur_func = nullptr;
	ClassAST* array_cls = nullptr;
	ClassAST* string_cls = nullptr;

	using ClassTemplateInstMap = std::map<std::reference_wrapper<const std::vector<TemplateArgument>>, std::unique_ptr<ClassAST>>;
	using FuncTemplateInstMap = std::map<std::reference_wrapper<const std::vector<TemplateArgument>>, std::unique_ptr<FunctionAST>>;
	std::vector<std::pair<ClassTemplateInstMap*, typename ClassTemplateInstMap::key_type>> class_templ_inst_rollback;
	std::vector<std::pair<FuncTemplateInstMap*, typename FuncTemplateInstMap::key_type>> func_templ_inst_rollback;
};

#define scope_mgr (preprocessing_state._scope_mgr)
#define cur_func (preprocessing_state._cur_func)

static PreprocessingState preprocessing_state;
BD_CORE_API std::reference_wrapper<FunctionAST> GetCurrentPreprocessedFunction()
{
	return std::reference_wrapper<FunctionAST>(*cur_func);
}

BD_CORE_API std::reference_wrapper<ClassAST> GetCurrentPreprocessedClass()
{
	if (scope_mgr.class_stack.empty())
		return std::reference_wrapper<ClassAST>(*(ClassAST*)nullptr);
	return std::reference_wrapper<ClassAST>(*scope_mgr.class_stack.back());
}

BD_CORE_API void PushClass(ClassAST* cls)
{
	return scope_mgr.PushClass(cls);
}

BD_CORE_API void PopClass()
{
	return scope_mgr.PopClass();
}

void ClearTemplateInstancesRollbackLogs()
{
	preprocessing_state.func_templ_inst_rollback.clear();
	preprocessing_state.class_templ_inst_rollback.clear();
}

BD_CORE_API void RollbackTemplateInstances()
{
	//erase the instances in reversed order, because they may depend on previous ones
	auto& funcs = preprocessing_state.func_templ_inst_rollback;
	for(auto itr = funcs.rbegin();itr!=funcs.rend();++itr)
	{
		auto& the_map = *itr->first;
		auto arg = itr->second;
		auto mapitr = the_map.find(arg);
		if(mapitr!=the_map.end())
			the_map.erase(mapitr);
	}
	auto& clazzes = preprocessing_state.class_templ_inst_rollback;
	for(auto itr = clazzes.rbegin();itr!=clazzes.rend();++itr)
	{
		auto& the_map = *itr->first;
		auto arg = itr->second;
		auto mapitr = the_map.find(arg);
		if(mapitr!=the_map.end())
			the_map.erase(mapitr);
	}
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
		throw CompileError(pos, "Cannot find the name: " + name);
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

//will set need_preserve_member if this expression is an identifier with implied this
static FunctionAST* GetFunctionFromExpression(ExprAST* expr,SourcePos Pos, unique_ptr<ExprAST>* & need_preserve_member)
{
	FunctionAST* func = nullptr;
	IdentifierExprAST*  iden = dyncast_resolve_anno<IdentifierExprAST>(expr);
	if (iden)
	{
		auto resolvedfunc = dyncast_resolve_anno<ResolvedFuncExprAST>(iden->impl.get());
		if(resolvedfunc)
			func = resolvedfunc->def;
		else if (auto resolvedfunc = dyncast_resolve_anno<MemberExprAST>(iden->impl.get()))
		{
			func = GetFunctionFromExpression(resolvedfunc, Pos, need_preserve_member);
			need_preserve_member = &resolvedfunc->Obj;
		}
	}
	else if (MemberExprAST*  mem = dyncast_resolve_anno<MemberExprAST>(expr))
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
	else if (auto resolved_func = dyncast_resolve_anno<ResolvedFuncExprAST>(expr))
	{
		func = resolved_func->def;
	}
	return func;
}

void ThrowCastError(ResolvedType& target, ResolvedType& fromtype, SourcePos pos)
{
	string msg = "Cannot cast from type ";
	msg += fromtype.GetString();
	msg += " to type ";
	msg += target.GetString();
	throw CompileError(pos, msg);
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
	case tok_short:
		return make_unique<CastNumberExpr<tok_short, typeto>>(std::move(val), pos);
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

bool Birdee::ClassAST::HasParent(Birdee::ClassAST* checkparent)
{
	ClassAST* cur = this;
	while (cur != checkparent)
	{
		cur = cur->parent_class;
		if (cur == nullptr)
			return false;
	}
	return true;
}

inline bool IsClosure(const ResolvedType& v)
{
	return v.index_level == 0 && v.type == tok_func && v.proto_ast->is_closure;
}
inline bool IsFunctype(const ResolvedType& v)
{
	return v.index_level == 0 && v.type == tok_func && !v.proto_ast->is_closure;
}

unique_ptr<ExprAST> FixTypeForAssignment(ResolvedType& target, unique_ptr<ExprAST>&& val,SourcePos pos)
{
	if (target == val->resolved_type)
	{
		return std::move(val);
	}
	if (IsClosure(target) && IsFunctype(val->resolved_type))
	{
		//if both types are functions and target is closure & source value is not closure
		if (target.proto_ast->IsSamePrototype(*val->resolved_type.proto_ast))
		{
			return make_unique<FunctionToClosureAST>(std::move(val));
		}
	}
	if (target.isReference())
	{
		if (val->resolved_type.isNull())
		{
			FixNull(val.get(), target);
			return std::move(val);
		}
		if (IsResolvedTypeClass(val->resolved_type)
			&& IsResolvedTypeClass(target)) // check for upcast
		{
			if (val->resolved_type.class_ast->HasParent(target.class_ast))
				return make_unique<UpcastExprAST>(std::move(val), target.class_ast, pos);
		}
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
		case tok_short:
			return fix_type(tok_short);
		}
	}
#undef fix_type
	ThrowCastError(target, val->resolved_type, pos);
	return nullptr;
}

static unordered_map<Token, int> promotion_map = {
{ tok_byte,-2 },
{ tok_short,-1 },
{ tok_int,0 },
{tok_uint,1},
{tok_long,2},
{tok_ulong,3},
{tok_float,4},
{tok_double,5},
};



static ResolvedType GetMoreGeneralType(ResolvedType& v1, ResolvedType& v2)
{
	if (v1 == v2)
	{
		return v1;
	}
	else if (IsClosure(v1) && IsFunctype(v2)) //v1 is closure, v2 is functype
	{
		//if both types are functions and target is closure & source value is not closure
		if (v1.proto_ast->IsSamePrototype(*v2.proto_ast))
			return v1;
	}
	else if (IsClosure(v2) && IsFunctype(v1)) //v2 is closure, v1 is functype
	{
		//if both types are functions and target is closure & source value is not closure
		if (v2.proto_ast->IsSamePrototype(*v1.proto_ast))
			return v2;
	}
	else if (v1.isReference())
	{
		if (v2.isNull())
			return v1;
		if (IsResolvedTypeClass(v2)
			&& IsResolvedTypeClass(v1)) // find the most recent shared ancestor class
		{
			vector<ClassAST*> parents1{v1.class_ast};
			ClassAST* cur = v1.class_ast->parent_class;
			while (cur)
			{
				parents1.push_back(cur);
				cur = cur->parent_class;
			}

			vector<ClassAST*> parents2{ v2.class_ast };
			cur = v2.class_ast->parent_class;
			while (cur)
			{
				parents2.push_back(cur);
				cur = cur->parent_class;
			}
			ClassAST* result = nullptr;
			for (auto itr1 = parents1.rbegin(), itr2 = parents2.rbegin();
				itr1 != parents1.rend() && itr2 != parents2.rend();
				itr1++, itr2++)
			{
				if (*itr1 != *itr2)
					break;
				result = *itr1;
			}
			if(!result)
				return ResolvedType();
			return ResolvedType(result);
		}
	}
	else if (v1.isNumber() && v1.isNumber())
	{
		int p1 = promotion_map[v1.type];
		int p2 = promotion_map[v2.type];
		if (p1 == p2)
			return v1;
		else if (p1 > p2)
			return v1;
		else
			return v2;
	}
	return ResolvedType();
}

Token PromoteNumberExpression(unique_ptr<ExprAST>& v1, unique_ptr<ExprAST>& v2,bool isBool, SourcePos pos)
{
	CompileAssert(v1->resolved_type.isNumber() && v2->resolved_type.isNumber(), pos, string("Cannot convert from type ")
		+ v1->resolved_type.GetString() + " to type " + v2->resolved_type.GetString());
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

extern string GetModuleNameByArray(const vector<string>& package, const char* delimiter = ".");

namespace Birdee
{
	void PushPyScope(ImportedModule* mod)
	{
		scope_mgr.py_scope_dicts.push_back(ScopeManager::PyScope(mod));
	}
	void PopPyScope()
	{
		scope_mgr.py_scope_dicts.pop_back();
	}
	BD_CORE_API string GetClassASTName(ClassAST* cls)
	{
		return cls->GetUniqueName();
	}

	static void ParseRawOneTemplateArg(unique_ptr<ExprAST>&& template_arg, vector<TemplateArgument>& this_template_args)
	{
			RunOnTemplateArg(template_arg.get(),
				[&this_template_args](BasicTypeExprAST* ex) {
				this_template_args.push_back(TemplateArgument(ResolvedType(*ex->type, ex->Pos)));
			},
				[&this_template_args](IdentifierExprAST* ex) {
				ImportTree* import_tree = nullptr;
				auto ret = scope_mgr.ResolveNameNoThrow(ex->Name, ex->Pos, import_tree);
				if (import_tree)
					assert(0 && "Not implemented");
				if (ret)
				{
					this_template_args.push_back(TemplateArgument(std::move(ret)));
				}
				else
				{
					IdentifierType type(ex->Name);
					Type& dummy = type;
					this_template_args.push_back(TemplateArgument(ResolvedType(dummy, ex->Pos)));
				}
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
					throw CompileError(ex->expr->Pos, "The template argument cannot be annotated");
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
					throw CompileError(ex->Expr->Pos, "The template argument cannot be annotated");
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
				[&this_template_args, &template_arg](ScriptAST* ex) {
				ex->Phase1();
				if (ex->stmt.size()) // if the user called set_ast(), use ScriptAST as an expression
				{
					CompileAssert(ex->stmt.size() == 1, ex->Pos,
						"You should call set_ast once in the script, because it is in a template argument");
					//if ex->resolved_type.isResolved(), then it's an expression. Otherwise, it's an general statement 
					CompileAssert(ex->resolved_type.isResolved(), ex->Pos,
						"The returned AST of this script must be an expression, because it is in a template argument");
					ParseRawOneTemplateArg(unique_ptr_cast<ExprAST>(std::move(ex->stmt.front())), this_template_args); //recursively parse the expression
				}
				else
				{
					CompileAssert(ex->type_data.isResolved(), ex->Pos,
						"This script must be either an expression or type, because it is in a template argument");
					this_template_args.push_back(TemplateArgument(ex->type_data));
				}
			},
				[&template_arg]() {
				throw CompileError(template_arg->Pos, "Invalid template argument expression type");
			}
			);
	}
	static void ParseRawTemplateArgs(vector<unique_ptr<ExprAST>>& raw_template_args, vector<TemplateArgument>& template_args)
	{
		for (auto& arg : raw_template_args)
		{
			ParseRawOneTemplateArg(std::move(arg), template_args);
		}
	}

	void* PyHandle::ptr()
	{
		return p;
	}

	void PyHandle::set(void* newp)
	{
		assert(!p);
		p = newp;
	}
	void PyHandle::reset()
	{
		if (p)
		{
			BirdeeDerefObj(p);
			p = nullptr;
		}
	}
	PyHandle::PyHandle()
	{
		p = nullptr;
	}
	PyHandle::PyHandle(PyHandle&& other) noexcept
	{
		p = other.p;
		other.p = nullptr;
	}

	PyHandle::PyHandle(void* pt)
	{
		p = pt;
	}

	PyHandle::~PyHandle()
	{
		if (p)
			BirdeeDerefObj(p);
	}

	BD_CORE_API void ClearPyHandles()
	{
		preprocessing_state.main_global_scope.reset();
		scope_mgr.py_scope_dicts.clear();
	}

	void GetPyScope(void*& globals, void*& locals)
	{
		auto& scope = scope_mgr.py_scope_dicts.back();
		//if we are in another module and the module has not a valid global pyscope, create one from "original scope"
		if (scope.mod)
		{
			if (!scope.mod->py_scope.ptr())
				scope.mod->py_scope.set(BirdeeCopyPyScope(BirdeeGetOrigScope()));
			globals = scope.mod->py_scope.ptr();
		}
		else
		{
			if(!preprocessing_state.main_global_scope.ptr())
				preprocessing_state.main_global_scope.set(BirdeeCopyPyScope(BirdeeGetOrigScope()));
			globals = preprocessing_state.main_global_scope.ptr();
		}
			
		//if we are in top-level, pass nullptr to scope_dict parameter
		if (scope.scope_dicts.empty())
			locals = nullptr;
		else
		{
			//if the current scope dict is not null (because we have already run some script in the current level)
			//use the ready scope_dict
			auto& handle = scope.scope_dicts.back();
			if (!handle.ptr())
			{
				//else, create a new scope dict
				//first find the first non-null dict in the scopes
				void* src_ptr = nullptr;
				for (int i = scope.scope_dicts.size()-1; i >= 0; i--)
				{
					if (scope.scope_dicts[i].ptr())
					{
						src_ptr = scope.scope_dicts[i].ptr();
						break;
					}
				}
				handle.set(BirdeeCopyPyScope(src_ptr));
			}
			locals = handle.ptr();
		}
	}

	string int2str(const int v)
	{
		std::stringstream stream;
		stream << v;
		return stream.str();
	}
	static void ValidateOneTemplateArg(const vector<TemplateArgument>& args, const vector<TemplateParameter>& params, const SourcePos& Pos, int i)
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
	static unique_ptr<vector<TemplateArgument>> DoTemplateValidateArguments(FunctionAST* src_template, bool is_vararg, const vector<TemplateParameter>& params,
		unique_ptr<vector<TemplateArgument>>&& pargs, SourcePos Pos, bool throw_if_vararg)
	{
		vector<TemplateArgument>& args = *pargs;
		if (!is_vararg)
		{
			if (params.size() != args.size())
			{
				throw TemplateArgumentNumberError(Pos,
					string("The template requires ") + int2str(params.size()) + " arguments, but " + int2str(args.size()) + " are given",
					src_template, std::move(pargs));
			}
		}
		else
		{
			if (params.size() > args.size() || (throw_if_vararg && params.size() == args.size()))
			{
				throw TemplateArgumentNumberError(Pos,
					string("The template requires at least ") + int2str(params.size()) + " arguments, but " + int2str(args.size()) + " are given",
					src_template, std::move(pargs));
			}
		}

		for (int i = 0; i < params.size(); i++)
		{
			ValidateOneTemplateArg(args, params, Pos, i);
		}
		return std::move(pargs);

	}


	template<>
	unique_ptr<vector<TemplateArgument>> Birdee::TemplateParameters<FunctionAST>::ValidateArguments(FunctionAST* ths, unique_ptr<vector<TemplateArgument>>&& args, SourcePos Pos, bool throw_if_vararg)
	{
		return DoTemplateValidateArguments(ths, is_vararg, params, std::move(args), Pos, throw_if_vararg);
	}

	template<>
	unique_ptr<vector<TemplateArgument>> Birdee::TemplateParameters<ClassAST>::ValidateArguments(ClassAST* ths, unique_ptr<vector<TemplateArgument>>&& args, SourcePos Pos, bool throw_if_vararg)
	{
		return DoTemplateValidateArguments(nullptr, is_vararg, params, std::move(args), Pos, throw_if_vararg);
	}

	//force specialize these two functions to make them exported in g++
	template<>
	FunctionAST * Birdee::TemplateParameters<FunctionAST>::GetOrCreate(unique_ptr<vector<TemplateArgument>>&& v, FunctionAST* source_template, SourcePos pos)
	{
		return GetOrCreateImpl(std::move(v), source_template, pos);
	}

	template<>
	ClassAST * Birdee::TemplateParameters<ClassAST>::GetOrCreate(unique_ptr<vector<TemplateArgument>>&& v, ClassAST* source_template, SourcePos pos)
	{
		return GetOrCreateImpl(std::move(v), source_template, pos);
	}

	void ResolvedType::ResolveType(Type& type, SourcePos pos)
	{
		if (type.type == tok_script)
		{
			void* globals, *locals;
			GetPyScope(globals, locals);
			Birdee_ScriptType_Resolve(this, static_cast<ScriptType*>(&type), pos, globals, locals);
			this->index_level += type.index_level;
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
				this->index_level += ty->index_level; //the new index level is the sum of the old levels
				//e.g. T = int[], then T[] should be int[][]
				return;
			}
			scope_mgr.isInTemplateOfOtherModuleAndDoImport();
			//if we are in a template and it is imported from another modules
			auto find_in_template_env = [&pos](ResolvedType& ths, const ScopeManager::TemplateEnv& env, const string& str)
			{
				//we are now acting as if we are compiling another module
				auto& functypemap = env.imported_mod->functypemap;
				if (auto fitr = FindModuleGlobal(functypemap, str, true))
				{
					ths.type = tok_func;
					ths.proto_ast = fitr;
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
				if (auto cls = FindModuleGlobal(classmap, str, true))
				{
					ths.type = tok_class;
					ths.class_ast = cls;
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
					this->proto_ast = fitr->second.first.get();
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
							this->class_ast = itr->second.first;
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
				throw CompileError(pos,"The module " + GetModuleNameByArray(ty->name) + " has not been imported");
			}
			auto& functypemap2 = node->mod->functypemap;
			if (auto fitr = FindModuleGlobal(functypemap2, clsname, false))
			{
				this->type = tok_func;
				this->proto_ast = fitr;
			}
			else
			{
				auto itr = FindModuleGlobal(node->mod->classmap, clsname, false);
				CompileAssert(itr, pos, "Cannot find type name " + clsname + " in module " + GetModuleNameByArray(ty->name));
				this->type = tok_class;
				this->class_ast = itr;
			}
		}
		if (this->type==tok_class &&(type.type == tok_identifier || type.type == tok_package))
		{
			GeneralIdentifierType* ty = static_cast<GeneralIdentifierType*>(&type);
			if (ty->template_args)
			{
				CompileAssert(class_ast->isTemplate(),pos, "The class must be a template class");
				auto arg= make_unique<vector<TemplateArgument>>();
				auto& args = *ty->template_args.get();
				ParseRawTemplateArgs(args, *arg);
				arg = class_ast->template_param->ValidateArguments(class_ast, std::move(arg), pos, false);
				class_ast = class_ast->template_param->GetOrCreate(std::move(arg), class_ast, pos); //will take the ownership of the pointer arg
			}
		}
		if(this->type==tok_class)
			CompileAssert(!this->class_ast->isTemplate(), pos, string("Cannot use template as a class instance: ") + this->class_ast->GetUniqueName());
	}

	void CompileUnit::Phase0()
	{
		for (auto& node : funcmap)
		{
			node.second.first->Phase0();
		}
		for (auto& node : classmap)
		{
			node.second.first->Phase0();
		}
		for (auto& node : functypemap)
		{
			node.second.first->Phase1(false);
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

	bool ResolvedType::isReference() const
	{
		if (index_level > 0 || type == tok_null)
			return true;
		if (type == tok_class)
			return !class_ast->is_struct;
		return false;
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


	bool IsSubResolvedType(const ResolvedType& parent, const ResolvedType& child)
	{
		if (type_is_class(parent))
		{
			if (!type_is_class(child))
				return false;
			return child.class_ast->HasParent(parent.class_ast);
		}
		return parent == child;
	}

	bool PrototypeAST::CanBeAssignedWith(const PrototypeAST& other) const
	{
		auto& ths = *this;
		assert(ths.resolved_type.isResolved() && other.resolved_type.isResolved());
		//check return type
		if (!IsSubResolvedType(ths.resolved_type, other.resolved_type))
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
			if (!IsSubResolvedType(t1, t2))
				return false;
		}
		return true;
	}
	std::size_t PrototypeAST::rawhash() const
	{
		const PrototypeAST* proto = this;
		size_t v = proto->resolved_type.rawhash() << 3; //return type
		//v ^= (uintptr_t)proto->cls; //belonging class
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
		//if (ths.cls != other.cls)
		//	return false;
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


	void ScriptAST::Phase1()
	{
		void* globals, *locals;
		GetPyScope(globals, locals);
		Birdee_ScriptAST_Phase1(this, globals, locals);
	}

	llvm::Value* Birdee::AnnotationStatementAST::GetLValue(bool checkHas)
	{
		CompileAssert(is_expr, Pos, "Cannot get LValue of an non-expression");
		return GetLValueNoCheckExpr(checkHas);
	}

	void ArrayInitializerExprAST::Phase1()
	{
		CompileAssert(values.size(), Pos, "Empty initializer for the array is not allowed");
		ResolvedType rty;
		int idx = 1;
		for (auto& v : values)
		{
			v->Phase1();
			if (rty.isResolved())
			{
				auto ret = GetMoreGeneralType(rty, v->resolved_type);
				CompileAssert(ret.isResolved(), Pos, string("Cannot infer the type of the array: Given the array type ")
					+ rty.GetString() + ", and the " + std::to_string(idx) + "-th element's type " + v->resolved_type.GetString());
				rty = ret;
			}
			else
			{
				rty = v->resolved_type;
			}
			idx += 1;
		}
		for (auto& v : values)
		{
			v = FixTypeForAssignment(rty, std::move(v), v->Pos);
		}
		rty.index_level++;
		resolved_type = rty;
		is_static = scope_mgr.IsInTopLevel() && scope_mgr.top_level_scopes.empty();
	}

	void Birdee::AnnotationStatementAST::Phase1()
	{
		void* globals, *locals;
		GetPyScope(globals, locals);
		if (isa<ClassAST>(impl.get()))
		{
			ClassAST* cls = static_cast<ClassAST*>(impl.get());
			if (cls->isTemplate())
			{
				for (auto& inst : cls->template_param->instances)
				{
					Birdee_RunAnnotationsOn(anno, inst.second.get(), inst.second->Pos, globals);
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
					Birdee_RunAnnotationsOn(anno, inst.second.get(), inst.second->Pos, globals);
				}
				func->template_param->annotation = this;
				return;
			}
			//if is not a template, fall through to the following code
		}
		impl->Phase1();
		Birdee_RunAnnotationsOn(anno,impl.get(),impl->Pos, globals);
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
			throw CompileError(expr->Pos, "The expression is neither NumberExprAST nor StringLiteralAST");
		}
		else if(s1)
		{
			if (s2)
				return s1->Val < s2->Val;
			if (n2)
				return false; 
			throw CompileError(expr->Pos, "The expression is neither NumberExprAST nor StringLiteralAST");
		}
		throw CompileError(expr->Pos, "The expression is neither NumberExprAST nor StringLiteralAST");
		return false;
	}

	void ClassAST::Phase0()
	{
		if (done_phase >= 1)
			return;
		done_phase = 1;
		if (isTemplate())
			return;
    
		if (isTemplateInstance())
		{//if is template instance, set member template func's mod
			assert(template_source_class);
			if (template_source_class->template_param->mod)
			{
				for (auto& funcdef : funcs)
				{
					if(funcdef.decl->template_param)
						funcdef.decl->template_param->mod = template_source_class->template_param->mod;
				}
			}
		}
    
		static std::vector<ClassAST*> loop_checker;
		if (std::find(loop_checker.begin(), loop_checker.end(), this) != loop_checker.end())
		{
			std::stringstream buf;
			buf << "A loop in the class inherience relations is detected:";
			for (auto& itr : loop_checker)
				buf << "->" << itr->GetUniqueName();
			throw CompileError(Pos, buf.str());
		}
		scope_mgr.class_stack.push_back(this);
		if (parent_type) //it is possible that parent_class!=null and parent_type==null, when the class is imported
		{
			// resolve parent type 
			auto parent_resolved_type = ResolvedType(*parent_type, Pos);
			CompileAssert(parent_resolved_type.type == tok_class, Pos, "Expecting a class as parent");
			parent_type = nullptr;
			parent_class = parent_resolved_type.class_ast;
		}
		if(parent_class) 
		{
			//call Phase0 on parent
			loop_checker.push_back(this);
			scope_mgr.class_stack.pop_back();
			parent_class->Phase0();
			scope_mgr.class_stack.push_back(this);
			loop_checker.pop_back();
			vtabledef = parent_class->vtabledef;
		}
		auto checkproto = [](FunctionAST* curfunc, FunctionAST* overriden) {
			//make sure the overriden function has the same prototype
			CompileAssert(overriden->Proto->CanBeAssignedWith(*curfunc->Proto), curfunc->Pos,
				string("The function ") + curfunc->Proto->Name + " overrides the function at " + overriden->Pos.ToString()
				+ " but they have different prototypes");
		};

		// automatically mark class as abstract if there's unimplemented pure virtual function
		{
			unordered_set<string> parent_funcdef;
			ClassAST* cls = parent_class;
			while (!this->is_abstract && cls)
			{
				for (auto& funcdef : cls->funcs) {
					string & name = funcdef.decl->Proto->Name;
					if (parent_funcdef.find(name) != parent_funcdef.end())
						continue;
					parent_funcdef.insert(name);
					if (funcdef.is_abstract && this->funcmap.find(name) == this->funcmap.end()) {
						this->is_abstract = true;
						break;
					}
				}
				cls = cls->parent_class;
			}
		}

		for (auto& funcdef : funcs) {
			if (funcdef.is_abstract) {
				ClassAST* cls = parent_class;
				while (cls)
				{
					auto itr = cls->funcmap.find(funcdef.decl->Proto->Name);
					if (itr != cls->funcmap.end())
					{
						CompileAssert(cls->funcs[itr->second].is_abstract, funcdef.decl->Pos, 
							"method " + funcdef.decl->Proto->Name + "is defind as abstract in class " +
							this->GetUniqueName() + ", but it's non-abstract in parent class " + cls->GetUniqueName());
						break;
					}
					cls = cls->parent_class;
				}
			}
		}

		for (auto& funcdef : funcs)
		{
			funcdef.decl->Phase0();
			if (funcdef.virtual_idx == MemberFunctionDef::VIRT_NONE)
			{
				//if the function is not manually marked virtual, check if it overrides a virtual function
				//if so, copy the virtual idx
				ClassAST* cls = parent_class;
				while (cls)
				{
					auto itr = cls->funcmap.find(funcdef.decl->Proto->Name);
					if (itr != cls->funcmap.end())
					{
						funcdef.virtual_idx = cls->funcs[itr->second].virtual_idx;
						break;
					}
					cls = cls->parent_class;
				}
			}
			//if it is marked a virtual function, set the virtual index and set the vtable
			if (funcdef.virtual_idx != MemberFunctionDef::VIRT_NONE)
			{
				if (funcdef.virtual_idx != MemberFunctionDef::VIRT_UNRESOLVED)
				{//if the virtual function is resolved (either is imported or overides a parent), the virtual function has already an index
					if (funcdef.virtual_idx >= 0 && funcdef.virtual_idx < vtabledef.size())
					{ //if it is an override function
						CompileAssert(funcdef.decl->Proto->Name == vtabledef[funcdef.virtual_idx]->Proto->Name,
							Pos, string("The virtual function ") + funcdef.decl->Proto->Name
							+ " overrides a function with different name " + vtabledef[funcdef.virtual_idx]->Proto->Name);
						checkproto(funcdef.decl.get(), vtabledef[funcdef.virtual_idx]);
						vtabledef[funcdef.virtual_idx] = funcdef.decl.get();
					}
					else if (funcdef.virtual_idx ==  vtabledef.size())
					{//if it is not overriding a parent function, the virtual idx should be same as the size of vtable
						vtabledef.push_back(funcdef.decl.get());
					}
					else
					{
						std::stringstream buf;
						buf << "The imported virtual function " << funcdef.decl->Proto->Name << " has a bad virtual index " << funcdef.virtual_idx;
						throw CompileError(Pos, buf.str());
					}
				}
				else //if the virtual function index has not been resolved
				{
					for (int i = 0; i < vtabledef.size(); i++)
					{
						if (funcdef.decl->Proto->Name == vtabledef[i]->Proto->Name)
						{
							checkproto(funcdef.decl.get(), vtabledef[i]);
							vtabledef[i] = funcdef.decl.get();
							funcdef.virtual_idx = i;
						}
					}
					//if no overriding parent functions
					if (funcdef.virtual_idx == MemberFunctionDef::VIRT_UNRESOLVED)
					{
						vtabledef.push_back(funcdef.decl.get());
						funcdef.virtual_idx = vtabledef.size() - 1;
					}
				}
			}
		}
		for (auto& fielddef : fields)
		{
			fielddef.decl->Phase0();
		}
		scope_mgr.class_stack.pop_back();
	}

	vector<string> MemberExprAST::ToStringArray()
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
		for (int i = reverse.size() - 1; i >= 0; i--)
		{
			ret.push_back(*reverse[i]);
		}
		return ret;
	}

	void FunctionAST::Phase0()
	{
		FunctionAST* template_func = template_source_func;
		const vector<TemplateArgument>* templ_args = template_instance_args.get();
		auto& Proto = this->Proto;
		auto& Pos = this->Pos;
		auto& is_vararg = this->is_vararg;
		if (isTemplate())
			return;
		if (resolved_type.isResolved())
			return;
		resolved_type.type = tok_func;
		resolved_type.index_level = 0;
		resolved_type.proto_ast = Proto.get();
		auto add_var_arg = [&Proto,&Pos,&is_vararg](const vector<TemplateArgument>& args, int start_idx)
		{
			is_vararg = false;
			for (int i = start_idx; i < args.size(); i++)
			{
				CompileAssert(args[i].kind == TemplateArgument::TEMPLATE_ARG_TYPE, Pos, string("The ") + int2str(i+1) +"-th vararg should be a type");
				auto arg_variable = make_unique< VariableSingleDefAST>(string("___vararg") + int2str(i - start_idx), args[i].type);
				arg_variable->Pos = Pos;
				Proto->resolved_args.push_back(std::move(arg_variable));
			}
			
		};
		auto prv_func = cur_func;
		cur_func = this;
		Proto->Phase0();
		cur_func = prv_func;
/*
vararg matching:
first match function template's vararg
then the classes'.
If using named vararg, the vararg declaration name must be the same as the vararg usage name
If usage vararg name is "", match the closest vararg
*/
		if (is_vararg && isTemplateInstance)
		{
			assert(templ_args && template_func);
			if (template_func->template_param->is_vararg)
			{
				if (vararg_name.empty() || vararg_name == template_func->template_param->vararg_name)
				{
					add_var_arg(*templ_args, template_func->template_param->params.size());
					//will set is_vararg=false
				}
			}
		}
		if (is_vararg && Proto->cls && Proto->cls->isTemplateInstance())
		{
			auto& src_templ_arg = Proto->cls->template_source_class->template_param;
			if (src_templ_arg->is_vararg)
			{
				if (vararg_name.empty() || vararg_name == src_templ_arg->vararg_name)
				{
					add_var_arg(*Proto->cls->template_instance_args, src_templ_arg->params.size());
					//will set is_vararg=false
				}
			}
		}
		CompileAssert(!is_vararg, Pos, string("Cannot resolve vararg parameter: \"") + vararg_name + "\"");
		if (this->is_extension)
		{
			assert(!Proto->cls);
			assert(!Proto->is_closure);
			CompileAssert(Proto->resolved_args.size() 
				&& IsResolvedTypeClass(Proto->resolved_args.front()->resolved_type), 
				Pos, string("The type of the class extension function's first parameter should be a class"));
			auto& funcname = Proto->GetName();
			auto pos = funcname.find('_');
			if (pos != string::npos)
			{
				pos += 1;
				CompileAssert(pos < funcname.length(), Pos, string("Bad class extension function name. It must have at lest 1 character after \'_\'"));
			}
			else
			{
				pos = 0;
			}
			size_t len = funcname.length() - pos;
			auto clsast = Proto->resolved_args.front()->resolved_type.class_ast;
			std::pair<ClassAST*, str_view> k = std::make_pair(clsast, str_view{&funcname, pos, len});
			auto olditr = cu.class_extend_funcmap.find(k);
			CompileAssert(olditr == cu.class_extend_funcmap.end(), Pos,
				string("The extension name ") + k.second.to_string()
				+ " for class " + clsast->GetUniqueName() +" has been already defined at " + olditr->second->GetName());
			std::pair< std::pair<ClassAST*, str_view>, FunctionAST*> v = std::make_pair(k, this);
			cu.class_extend_funcmap.insert(v);
		}
	}
	void FunctionTemplateInstanceExprAST::Phase1()
	{
		Phase1(false);
	}
	void FunctionTemplateInstanceExprAST::Phase1(bool is_in_call)
	{
		FunctionAST* func = nullptr;
		expr->Phase1();
		unique_ptr<ExprAST>* dummy;
		func = GetFunctionFromExpression(expr.get(),Pos, dummy);
		CompileAssert(func, Pos, "Expected a function name or a member function for template");
		CompileAssert(func->isTemplate(), Pos, "The function is not a template");
		auto template_args = make_unique<vector<TemplateArgument>>();
		ParseRawTemplateArgs(raw_template_args, *template_args);
		raw_template_args.clear();
		template_args = func->template_param->ValidateArguments(func, std::move(template_args), Pos, is_in_call);
		instance = func->template_param->GetOrCreate(std::move(template_args), func, Pos);
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

	static void Phase1ForTemplateInstance(FunctionAST* func, FunctionAST* src_func, unique_ptr<vector<TemplateArgument>>&& v,
		const vector<TemplateParameter>& parameters,ImportedModule* mod, SourcePos pos)
	{
		preprocessing_state.func_templ_inst_rollback.push_back(
			std::make_pair(&src_func->template_param->instances, 
			std::reference_wrapper<const std::vector<TemplateArgument>>(*v)));
		func->Proto->Name += GetTemplateArgumentString(*v);
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
		scope_mgr.SetTemplateEnv(*v, parameters, mod, pos);
		scope_mgr.template_trace_back_stack.push_back(std::make_pair(&scope_mgr.template_stack, scope_mgr.template_stack.size() - 1));
		vector<ClassAST*> clsstack;
		if(!cls_template) // if it is not a member function, clear the class environment
			clsstack = std::move(scope_mgr.class_stack);
		auto basic_blocks_backup = std::move(scope_mgr.function_scopes);
		scope_mgr.function_scopes = vector <ScopeManager::FunctionScope>();
		func->isTemplateInstance = true;
		func->template_instance_args = std::move(v);
		func->template_source_func = src_func;
		func->Phase0();
		func->Phase1();
		if (!cls_template)
			scope_mgr.class_stack = std::move(clsstack);
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
	static void Phase1ForTemplateInstance(ClassAST* cls, ClassAST* src_cls, unique_ptr<vector<TemplateArgument>>&& v,
		const vector<TemplateParameter>& parameters,ImportedModule* mod, SourcePos pos)
	{
		preprocessing_state.class_templ_inst_rollback.push_back(
			std::make_pair(&src_cls->template_param->instances, 
			std::reference_wrapper<const std::vector<TemplateArgument>>(*v)));
		auto& args = *v;
		cls->template_instance_args = std::move(v);
		cls->template_source_class = src_cls;
		cls->name += GetTemplateArgumentString(args);
		scope_mgr.SetClassTemplateEnv(args, parameters, mod, pos);
		scope_mgr.SetEmptyTemplateEnv(ScopeManager::PyScope(mod));
		scope_mgr.template_trace_back_stack.push_back(std::make_pair(&scope_mgr.template_class_stack, scope_mgr.template_class_stack.size() - 1));
		for (auto& funcdef : cls->funcs)
		{
			funcdef.decl->Proto->cls = cls;
		}
		cls->Phase0();
		cls->Phase1();
		scope_mgr.template_trace_back_stack.pop_back();
		scope_mgr.RestoreTemplateEnv();
		scope_mgr.RestoreClassTemplateEnv();
	}

	template<typename T>
	T * Birdee::TemplateParameters<T>::GetOrCreateImpl(unique_ptr<vector<TemplateArgument>>&& v, T* source_template, SourcePos pos)
	{
		auto ins = instances.find(*v);
		if (ins != instances.end())
		{
			return ins->second.get();
		}
		unique_ptr<T> replica_func = source_template->CopyNoTemplate();
		T* ret = replica_func.get();
		instances.insert(std::make_pair(reference_wrapper<const vector<TemplateArgument>>(*v), std::move(replica_func)));
		auto old_f_scopes = std::move(scope_mgr.function_scopes);
		Phase1ForTemplateInstance(ret, source_template, std::move(v), params, mod, pos);
		scope_mgr.function_scopes = std::move(old_f_scopes);
		if (annotation)
		{
			PushPyScope(mod);
			void* globals, *locals;
			GetPyScope(globals, locals);
			Birdee_RunAnnotationsOn(annotation->anno,ret,ret->Pos,globals);
			PopPyScope();
		}
		return ret;
	}

	ResolvedType ResolveClassMember(ExprAST* obj, const string& member, const SourcePos& Pos,
		/*out parameters*/ int& casade_parents,
		MemberExprAST::MemberType& kind, MemberFunctionDef*& thsfunc, FieldDef*& thsfield, FunctionAST*& thsextension)
	{
		ClassAST* cur_cls = obj->resolved_type.class_ast;
		while (cur_cls) {
			auto field = cur_cls->fieldmap.find(member);
			if (field != cur_cls->fieldmap.end())
			{
				CompileAssert(cur_cls->fields[field->second].access == access_public || scope_mgr.IsCurrentClass(cur_cls), // if is private and we are not in the class
					Pos, string("Accessing a private member") + member + " outside of a class");
				kind = MemberExprAST::member_field;
				thsfield = &(cur_cls->fields[field->second]);
				return thsfield->decl->resolved_type;
			}
			auto func = cur_cls->funcmap.find(member);
			if (func != cur_cls->funcmap.end())
			{
				thsfunc = &(cur_cls->funcs[func->second]);
				if (obj && dyncast_resolve_anno<SuperExprAST>(obj)) //if the object is "super", do not generate virtual call here
					kind = MemberExprAST::member_function;
				else
					kind = thsfunc->virtual_idx == MemberFunctionDef::VIRT_NONE ? MemberExprAST::member_function : MemberExprAST::member_virtual_function;
				CompileAssert(thsfunc->access == access_public || scope_mgr.IsCurrentClass(cur_cls), // if is private and we are not in the class
					Pos, string("Accessing a private member") + member + " outside of a class");
				return thsfunc->decl->resolved_type;
			}
			auto extended = cu.class_extend_funcmap.find(std::make_pair(cur_cls, str_view{ &member, 0, member.length() }));
			if (extended != cu.class_extend_funcmap.end())
			{
				kind = MemberExprAST::member_imported_function;
				thsextension = extended->second;
				// now generate a new prototype ast for the member expr
				ResolvedType rty;
				rty.type = tok_func;
				rty.proto_ast = scope_mgr.GetExtensionFunctionPrototype(thsextension);
				return rty;
			}
			casade_parents++;
			cur_cls = cur_cls->parent_class;
		}
		return ResolvedType();
	}

	//returns the casade_parents & member definition of the member
	//returns <-1,nullptr> if not found
	BD_CORE_API std::pair<int, FieldDef*> FindClassField(ClassAST* class_ast, const string& member)
	{
		int casade_parents = 0;
		ClassAST* cur_cls = class_ast;
		while (cur_cls) {
			auto field = cur_cls->fieldmap.find(member);
			if (field != cur_cls->fieldmap.end())
			{
				auto thsfield = &(cur_cls->fields[field->second]);
				return std::make_pair(casade_parents, thsfield);
			}
			casade_parents++;
			cur_cls = cur_cls->parent_class;
		}
		return std::make_pair(-1, nullptr);
	}

	//returns the casade_parents & member definition of the member
	//returns <-1,nullptr> if not found
	BD_CORE_API std::pair<int, MemberFunctionDef*> FindClassMethod(ClassAST* class_ast, const string& member)
	{
		int casade_parents = 0;
		ClassAST* cur_cls = class_ast;
		while (cur_cls) {
			auto field = cur_cls->funcmap.find(member);
			if (field != cur_cls->funcmap.end())
			{
				auto thsfield = &(cur_cls->funcs[field->second]);
				return std::make_pair(casade_parents, thsfield);
			}
			casade_parents++;
			cur_cls = cur_cls->parent_class;
		}
		return std::make_pair(-1, nullptr);
	}
	static void RunPhase0OnClass(ClassAST* classast)
	{
		if (classast->done_phase < 1)
		{
			assert(!classast->isTemplateInstance());
			scope_mgr.SetEmptyClassTemplateEnv();
			scope_mgr.SetEmptyTemplateEnv(ScopeManager::PyScope(nullptr));
			classast->Phase0();
			scope_mgr.RestoreTemplateEnv();
			scope_mgr.RestoreClassTemplateEnv();
		}
	}

	// if the member function is defined within the scope of the class
	inline bool IsInternalMemberFunction(MemberExprAST::MemberType kind)
	{
		return kind == MemberExprAST::member_function || kind == MemberExprAST::member_virtual_function;
	}

	// if the memberexpr is a function
	inline bool IsMemberAFunction(MemberExprAST::MemberType kind)
	{
		return IsInternalMemberFunction(kind) || kind == MemberExprAST::member_imported_function;
	}

	static unique_ptr<MemberExprAST> ResolveAndCreateMemberFunctionExpr(unique_ptr<ExprAST>&& obj, const string& name, SourcePos& Pos)
	{
		auto classast = obj->resolved_type.class_ast;
		RunPhase0OnClass(classast);
		int cascade_parents = 0;
		MemberExprAST::MemberType kind = MemberExprAST::MemberType::member_error;
		MemberFunctionDef* outfunc = nullptr;
		FieldDef* outfield = nullptr;
		FunctionAST* outextension = nullptr;
		auto rty = ResolveClassMember(obj.get(), name, Pos, cascade_parents, kind, outfunc, outfield, outextension);
		CompileAssert(IsMemberAFunction(kind),
			Pos, string("Cannot find public method") + name + " in class " + classast->GetUniqueName() + " or it is a field.");
		unique_ptr<MemberExprAST> memberexpr;
		if (IsInternalMemberFunction(kind))
		{
			memberexpr = make_unique<MemberExprAST>(std::move(obj), outfunc, Pos);
		}
		else
		{
			memberexpr = make_unique<MemberExprAST>(std::move(obj), name);
			memberexpr->import_func = outextension;
			memberexpr->Pos = Pos;
			memberexpr->resolved_type = rty;
		}
		memberexpr->kind = kind;
		memberexpr->casade_parents = cascade_parents;
		return std::move(memberexpr);
	}


	bool IndexExprAST::isOverloaded()
	{
		if (!Expr->resolved_type.isResolved())
			Expr->Phase1();
		return 	Expr->resolved_type.index_level==0 && Expr->resolved_type.type == tok_class;
	}

	bool IndexExprAST::isTemplateInstance(unique_ptr<ExprAST>*& member)
	{
		if (!Expr)//if the expr is moved, check if "instance"  is callexpr. If so, it is an overloaded call to __getitem__
			return !(isa<CallExprAST>(instance.get()));
		auto func = GetFunctionFromExpression(Expr.get(), Pos, member);
		if (func && func->isTemplate())
		{
			return true;
		}
		return false;
	}
	void IndexExprAST::Phase1()
	{
		Phase1(false);
	}
	void IndexExprAST::Phase1(bool is_in_call)
	{
		if (!Expr || !Index) //if is resolved
			return;
		if(!Expr->resolved_type.isResolved())
			Expr->Phase1();
		unique_ptr<ExprAST>* member=nullptr;
		if(isTemplateInstance(member))
		{
			unique_ptr<ExprAST> ths_ptr;
			if (member) //if the expression is an identifier with implied "this", build a memberexpr
			{
				ths_ptr = std::move(*member);
			}
			vector<unique_ptr<ExprAST>> arg;
			arg.push_back(std::move(Index));
			instance = make_unique<FunctionTemplateInstanceExprAST>(std::move(Expr), std::move(arg),Pos);
			Expr = nullptr;
			auto inst = (FunctionTemplateInstanceExprAST*)instance.get();
			inst->Phase1(is_in_call);
			if (member) //if the expression is an identifier with implied "this", build a memberexpr
			{
				auto minst = make_unique<MemberExprAST>(std::move(ths_ptr), "");
				minst->kind = MemberExprAST::member_imported_function;
				minst->import_func = inst->instance;
				minst->resolved_type = inst->instance->resolved_type;
				instance = std::move(minst);
			}
			//else
			//	instance = std::move(inst);
			resolved_type = instance->resolved_type;
			return;
		}
		if (Expr->resolved_type.index_level==0 && Expr->resolved_type.type == tok_class)
		{
			auto memberexpr = ResolveAndCreateMemberFunctionExpr(std::move(Expr), "__getitem__", Pos);
			vector<unique_ptr<ExprAST>> args; args.emplace_back(std::move(Index));
			instance = make_unique<CallExprAST>(std::move(memberexpr), std::move(args));
			instance->Pos = Pos;
			//Index->Phase1 will be called in CallExprAST
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

	extern IntrisicFunction* FindIntrinsic(FunctionAST* func);

	void FunctionAST::Phase1()
	{
		if (isTemplate())
			return;
		//check if the function is intrinsic
		if (!Proto->cls)
		{
			intrinsic_function = FindIntrinsic(this);
			if (intrinsic_function)
			{
				Phase0();
				intrinsic_function->Phase1(this);
				return;
			}
		}

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
		if (captured_parent_super)
		{
			for (int i = 0; i <= scope_mgr.function_scopes.size() - 2; i++)
				scope_mgr.function_scopes[i].func->capture_super = true;
		}
		if (!imported_captured_var.empty() || captured_parent_this || captured_parent_super) //if captured any parent var, set the function type: is_closure=true
			Proto->is_closure = true;
		cur_func = prv_func;
		scope_mgr.function_scopes.pop_back();
	}

	void VariableSingleDefAST::Phase1InClass()
	{
		Phase0();
		if (resolved_type.type == tok_auto)
		{
			throw CompileError(Pos, "Member field of class must be defined with a type");
		}
		else
		{
			if (val.get())
			{
				throw CompileError(Pos, "Member field of class cannot have initializer");
				//val->Phase1();
				//val = FixTypeForAssignment(resolved_type, std::move(val), Pos);
			}
		}
	}

	ClassAST* GetArrayClass()
	{
		string name("genericarray");
		if (cu.is_corelib)
			return cu.classmap.find(name)->second.first;
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
			if(!arg->resolved_type.isResolved())
				arg->Phase1();
			SourcePos pos = arg->Pos;
			arg = FixTypeForAssignment(proto->resolved_args[i]->resolved_type, std::move(arg), pos);
			i++;
		}
	}

	static FunctionAST* FixFunctionForCall(unique_ptr<ExprAST>& Callee, std::vector<unique_ptr<ExprAST>>& Args, const SourcePos& Pos);

	static FunctionAST* FixFunctionForNew(MemberFunctionDef* funcdef, vector<unique_ptr<ExprAST>>& args ,ClassAST* cls, SourcePos Pos)
	{
		FunctionAST* func;
		if (!funcdef->decl->isTemplate())
		{
			func = funcdef->decl.get();
		}
		else
		{
			unique_ptr<ExprAST> memb = make_unique<ResolvedFuncExprAST>(funcdef->decl.get(), Pos);
			//check for template
			func = FixFunctionForCall(memb, args, Pos);
			assert(cls->HasParent(func->Proto->cls));
		}
		CheckFunctionCallParameters(func->Proto.get(), args, Pos);
		return func;
	}
	void NewExprAST::Phase1()
	{
		if (!resolved_type.isResolved())
		{
			resolved_type = ResolvedType(*ty, Pos);
		}
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
			CompileAssert(!resolved_type.class_ast->is_struct, Pos, "cannot apply new on a struct type");
			CompileAssert(!resolved_type.class_ast->is_abstract, Pos, "cannot instantiate abstract class");
			ClassAST* cls = resolved_type.class_ast;
			int cascade_parents = 0;
			MemberExprAST::MemberType kind= MemberExprAST::MemberType::member_error;
			MemberFunctionDef* outfunc = nullptr;
			FieldDef* outfield = nullptr;
			FunctionAST* outextension = nullptr;
			if (!method.empty())
			{
				auto rty = ResolveClassMember(this, method, Pos, cascade_parents, kind, outfunc, outfield, outextension);
				CompileAssert(IsInternalMemberFunction(kind),
					Pos, string("Cannot find public method") + method + " in class " + cls->GetUniqueName() + " or it is a field or a function.");
				//note: we ignore the @virtual flag here, since we know exactly the class of which we "new" an object.
				func = FixFunctionForNew(outfunc, args, resolved_type.class_ast, Pos);
			} else { // no specifically calling __init__, complier tries to find one
				auto rty = ResolveClassMember(this, "__init__", Pos, cascade_parents, kind, outfunc, outfield, outextension);
				if (IsInternalMemberFunction(kind))
					func = FixFunctionForNew(outfunc, args, resolved_type.class_ast, Pos);
				else
					CompileAssert(args.size() == 0, Pos, string("Cannot find the method __init__ in class ") + cls->GetUniqueName());
			}
		}
	}

	ClassAST* GetTypeInfoClass()
	{
		string name("type_info");
		if (cu.is_corelib)
			return cu.classmap.find(name)->second.first;
		else
		{
			return cu.imported_packages.FindName("birdee")->mod->classmap.find(name)->second.first.get();
		}
	}

	ThrowAST::ThrowAST(unique_ptr<ExprAST>&& expr, SourcePos pos) :expr(std::move(expr))
	{
		Pos = pos;
	}

	void ThrowAST::Phase1()
	{
		expr->Phase1();
		CompileAssert(expr->resolved_type.type == tok_class && expr->resolved_type.class_ast->needs_rtti,
			Pos, "The object to be thrown must be a class object reference with runtime type info");
	}

	//copy parent's "needs_rtti" bit (if exists)
	//fix-me: we can have compiler better performance here
	static bool ResolveRTTIBitInClass(ClassAST* cls)
	{
		if (!cls->needs_rtti && cls->parent_class)
		{
			//if !needs_rtti then it is either not resolved or the class really does not need rtti
			cls->needs_rtti = ResolveRTTIBitInClass(cls->parent_class);
		}
		return cls->needs_rtti;
	}

	void ClassAST::Phase1()
	{
		if (done_phase >= 2)
			return;
		done_phase = 2;
		if (isTemplate())
			return;
		Phase0();
		//copy parent's "needs_rtti" bit
		ResolveRTTIBitInClass(this);
		if (parent_class && needs_rtti)
		{
			//if this class has rtti, the parent must have rtti too
			CompileAssert(parent_class->needs_rtti, Pos,
				string("The super class ") + parent_class->GetUniqueName() + " of the class " + GetUniqueName() + " must have rtti.");
		}
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
				auto ret1 = FindModuleGlobal(node->mod->dimmap, member, false);
				if (ret1)
				{
					kind = member_imported_dim;
					import_dim = ret1;
					Obj = nullptr;
					resolved_type = ret1->resolved_type;
					return;
				}
				auto ret2 = FindModuleGlobal(node->mod->funcmap, member, false);
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
				throw CompileError(Pos,"Cannot resolve name "+ member);
			}
			else
			{
				kind = member_package;
				resolved_type.type = tok_package;
				resolved_type.import_node = node->FindName(member);
				if(!resolved_type.import_node)
					throw CompileError(Pos, "Cannot resolve name " + member);
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
					throw CompileError(Pos, "Accessing a private member outside of a class");
				resolved_type = this->func->decl->resolved_type;
				return;
			}
			throw CompileError(Pos, "Cannot find member " + member);
			return;
		}
		CompileAssert(Obj->resolved_type.type == tok_class, Pos, "The expression before the member should be an object");
		this->kind = MemberType::member_error;
		resolved_type = ResolveClassMember(Obj.get(),this->member, Pos, this->casade_parents, this->kind, this->func, this->field, this->import_func);
		CompileAssert(this->kind != MemberType::member_error, Pos, string("Cannot find member ") + member);
	}
	string TemplateArgument::GetString() const
	{
		if (kind == TEMPLATE_ARG_EXPR)
		{
			return "expression()";
		}
		return string("type(")+type.GetString()+")";
	}

	static void DeduceTemplateArgFromArg(Type* rawty, ResolvedType& rty, unordered_map<string, TemplateArgument>& name2type, const SourcePos& Pos);
	static void DeduceTemplateArgFromTemplateArgExpr(ExprAST* targ, ResolvedType& rty, unordered_map<string, TemplateArgument>& name2type, const SourcePos& Pos);
	static void DeduceTemplArgFromTemplArgExprList(vector<unique_ptr<ExprAST>>& raw_args, vector<TemplateArgument>&ty_args,
		unordered_map<string, TemplateArgument>& name2type,  const SourcePos& Pos)
	{
		if (raw_args.size() != ty_args.size())
			return;
		for (int i = 0; i < ty_args.size(); i++)
		{
			if (ty_args[i].kind == TemplateArgument::TEMPLATE_ARG_TYPE)
			{
				DeduceTemplateArgFromTemplateArgExpr(raw_args[i].get(), ty_args[i].type, name2type, Pos);
			}
		}
	}

	static void DeduceTemplateArgFromTemplateArgExpr(ExprAST* targ, ResolvedType& rty, unordered_map<string, TemplateArgument>& name2type, const SourcePos& Pos)
	{
		RunOnTemplateArg(targ,
			[&rty, &name2type, &Pos](BasicTypeExprAST* expr) {
			DeduceTemplateArgFromArg(expr->type.get(), rty, name2type, Pos);
		},
			[&rty,&name2type,&Pos](IdentifierExprAST* iden){
			IdentifierType ity(iden->Name);
			DeduceTemplateArgFromArg(&ity, rty, name2type, Pos);
		},
			[](MemberExprAST*) {},
			[&rty, &name2type, &Pos](FunctionTemplateInstanceExprAST* templ) {
			if (rty.type != tok_class || !rty.class_ast->isTemplateInstance())
				return;
			DeduceTemplArgFromTemplArgExprList(templ->raw_template_args, 
				*rty.class_ast->template_instance_args, name2type, Pos);		
		},
			[&rty, &name2type, &Pos](IndexExprAST* expr) {
			if (rty.type != tok_class || !rty.class_ast->isTemplateInstance())
				return;
			if (rty.class_ast->template_instance_args->size() != 1)
				return;
			auto& ty_args = *rty.class_ast->template_instance_args;
			if (ty_args[0].kind == TemplateArgument::TEMPLATE_ARG_TYPE)
				DeduceTemplateArgFromTemplateArgExpr(expr->Index.get(), ty_args[0].type, name2type, Pos);
		},
			[](NumberExprAST*) {},
			[](StringLiteralAST*) {},
			[](ScriptAST*) {},
			[]() {}
		);
	}

	static void DeduceTemplateArgFromArg(Type* rawty, ResolvedType& rty, unordered_map<string, TemplateArgument>& name2type, const SourcePos& Pos)
	{
		assert(rawty);
		if (rawty->index_level > rty.index_level)
			return;
		if (rawty->type == tok_identifier) {
			auto identifierType = static_cast<IdentifierType*>(rawty);
			if (identifierType->template_args)
			{
				//if arg is T[...]
				if (rawty->index_level != rty.index_level)
					return;
				if (rty.type != tok_class || !rty.class_ast->isTemplateInstance())
					return;
				DeduceTemplArgFromTemplArgExprList(*identifierType->template_args, *rty.class_ast->template_instance_args, name2type, Pos);
			}
			else
			{
				//if arg is plain identifier
				auto it = name2type.find(identifierType->name);
				auto result = rty;
				result.index_level -= rawty->index_level;
				//T[] matches int[][], where T=int[]
				if (it == name2type.end())
					name2type[identifierType->name] = result;
				else if (it->second.kind != TemplateArgument::TEMPLATE_ARG_TYPE || !(it->second.type == result))
					CompileAssert(false, Pos, string("Cannot derive template parameter type ") + identifierType->name
						+ " from given arguments: conflicting argument - " + it->second.GetString() + " and " + result.GetString());
			}

		}
		else if (rawty->type == tok_func)
		{
			if (rawty->index_level != rty.index_level)
				return;
			auto proto = static_cast<PrototypeType*>(rawty)->proto.get();
			if (rty.type != tok_func)
				return;
			DeduceTemplateArgFromArg(proto->RetType.get(), rty.proto_ast->resolved_type, name2type, Pos);
			auto args = proto->Args.get();
			auto param_size = rty.proto_ast->resolved_args.size();
			if (isa<VariableMultiDefAST>(args))
			{
				auto& lst = static_cast<VariableMultiDefAST*>(args)->lst;
				for (int i = 0; i < lst.size(); i++)
				{
					if (i >= param_size)
						return;
					DeduceTemplateArgFromArg(lst[i]->type.get(), rty.proto_ast->resolved_args[i]->resolved_type, name2type, Pos);
				}
			}
			else
			{
				assert(isa<VariableSingleDefAST>(args));
				if (param_size == 0)
					return;
				DeduceTemplateArgFromArg(static_cast<VariableSingleDefAST*>(args)->type.get(),
					rty.proto_ast->resolved_args[0]->resolved_type, name2type, Pos);
			}
		}
	}

	
	//Deduce the template arguments & vararg. If this function sucessfully deduced the function template,
	//it will return the template instance FunctionAST. Else, if the callee is a well defined function template instance,
	//e.g. somefunc[int,float], the instance will be returned. Otherwise, null is returned.
	//Param: 
	//	- Callee: the callee. Will be modified to an approriate AST node if callee should be deduced to a template instance
	//	- Args, Pos
	static FunctionAST* FixFunctionForCall(unique_ptr<ExprAST>& Callee, std::vector<unique_ptr<ExprAST>>& Args, const SourcePos& Pos)
	{
		FunctionAST* func = nullptr;
		unique_ptr<ExprAST>* preserved_member_obj = nullptr;
		unordered_map<string, TemplateArgument> name2type;
		try
		{
			if (auto indexexpr = dyncast_resolve_anno<IndexExprAST>(Callee.get()))
			{
				if(auto member = dyncast_resolve_anno<MemberExprAST>(indexexpr->Expr.get()))
					preserved_member_obj = &member->Obj;
				indexexpr->Phase1(true);
				if (auto idxexpr = dyncast_resolve_anno<FunctionTemplateInstanceExprAST>(indexexpr->instance.get()))
					func = idxexpr->instance;
			}
			else if (auto templexpr = dyncast_resolve_anno<FunctionTemplateInstanceExprAST>(Callee.get()))
			{
				if(auto member = dyncast_resolve_anno<MemberExprAST>(templexpr->expr.get()))
					preserved_member_obj = &member->Obj;
				templexpr->Phase1(true);
				func = templexpr->instance;
			}
			else
			{
				Callee->Phase1();
				func = GetFunctionFromExpression(Callee.get(), Pos, preserved_member_obj);
			}
		}
		catch (TemplateArgumentNumberError& e)
		{
			if (!e.src_template)
				throw e;
			func = e.src_template;
			assert(func->isTemplate());
			CompileAssert(func->is_vararg || e.args->size() <= func->template_param->params.size(), Pos,
				"Too many template arguments are given");
			for (int i = 0; i < e.args->size(); i++)
			{
				ValidateOneTemplateArg(*e.args, func->template_param->params, Pos, i);
				name2type[func->template_param->params[i].name] = std::move(e.args->at(i));
			}
		}

		if (func && func->isTemplate()) {
			// tries to derive template arguments from Args
			auto args = func->Proto->Args.get();
			vector<VariableSingleDefAST*> singleArgs;
			if (isa<VariableSingleDefAST>(args)) {
				auto singleArg = static_cast<VariableSingleDefAST*>(args);
				singleArgs.push_back(singleArg);
			}
			else {
				assert(isa<VariableMultiDefAST>(args));
				auto multiArg = static_cast<VariableMultiDefAST*>(args);
				for (auto & singleArg : multiArg->lst) {
					singleArgs.push_back(singleArg.get());
				}
			}

			if (func->is_vararg)
			{
				if (singleArgs.size() > Args.size()) {
					std::stringstream buf;
					buf << "The function requires at least" << singleArgs.size() << " arguments, but " << Args.size() << " are given";
					CompileAssert(false, Pos, buf.str());
				}
			}
			else
			{
				if (singleArgs.size() != Args.size()) {
					std::stringstream buf;
					buf << "The function requires " << singleArgs.size() << " arguments, but " << Args.size() << " are given";
					CompileAssert(false, Pos, buf.str());
				}
			}

			for (int i = 0; i < singleArgs.size(); i++) {
				Args[i]->Phase1();
				DeduceTemplateArgFromArg(singleArgs[i]->type.get(), Args[i]->resolved_type, name2type, Pos);
			}
			auto & params = func->template_param->params;
			// construct FunctionTemplateInstanceAST to replace Callee
			auto template_args = make_unique<vector<TemplateArgument>>();
			for (auto & param : params) {
				//CompileAssert(param.isTypeParameter(), Pos, "Cannot derive the value template parameter: " + param.name);
				auto itr = name2type.find(param.name);
				CompileAssert(itr != name2type.end(), Pos, "Cannot derive the template parameter: " + param.name);
				template_args->emplace_back(std::move(itr->second));
			}
			if (func->is_vararg)
			{
				//push the extra vararg types to the template args
				for (int i = singleArgs.size(); i < Args.size(); i++)
				{
					Args[i]->Phase1();
					template_args->emplace_back(TemplateArgument(Args[i]->resolved_type));
				}
			}
			template_args = func->template_param->ValidateArguments(func, std::move(template_args), Pos, false);
			auto instance = func->template_param->GetOrCreate(std::move(template_args), func, Pos);
			if (auto memb = dyncast_resolve_anno<MemberExprAST>(Callee.get()))
			{
				memb->kind = MemberExprAST::member_imported_function;
				memb->import_func = instance;
				memb->resolved_type = instance->resolved_type;
			}
			else if (preserved_member_obj)
			{
				auto minst = make_unique<MemberExprAST>(std::move(*preserved_member_obj), "");
				minst->kind = MemberExprAST::member_imported_function;
				minst->import_func = instance;
				minst->resolved_type = instance->resolved_type;
				Callee = std::move(minst);
			}
			else
			{
				Callee = make_unique<ResolvedFuncExprAST>(instance, Callee->Pos);
				Callee->Phase1();
			}
			func = instance;
			// do phase1 again
		}
		return func;
	}

	void CallExprAST::Phase1()
	{
		func_callee = FixFunctionForCall(Callee, Args, Pos);
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

	void SuperExprAST::Phase1()
	{
		CompileAssert(scope_mgr.class_stack.size() > 0, Pos, "Cannot reference \"super\" outside of a class");
		CompileAssert(!scope_mgr.class_stack.back()->is_struct, Pos, "Cannot reference \"super\" inside of a struct");
		CompileAssert(scope_mgr.class_stack.back()->parent_class, Pos, "Class does not have a parent to reference");
		if (scope_mgr.function_scopes.size() > 1) //if we are in a lambda func
		{
			scope_mgr.function_scopes.back().func->captured_parent_super = (llvm::Value*)1;
		}
		resolved_type.type = tok_class;
		resolved_type.class_ast = scope_mgr.class_stack.back()->parent_class;
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
		assert(cur_func->Proto->resolved_type.type != tok_error && "The prototype should be resolved first");
		if (Val)
		{
			Val->Phase1();
			Val = FixTypeForAssignment(cur_func->Proto->resolved_type, std::move(Val), Pos);
		}
	}

	ClassAST* GetStringClass()
	{
		string name("string");
		if (cu.is_corelib)
			return cu.classmap.find(name)->second.first;
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
		if (resolved_type.isResolved())
			return;
		auto& LHS = this->LHS;//just for lambda capture
		auto& RHS = this->RHS;
		auto& resolved_type = this->resolved_type;
		auto& Pos = this->Pos;
		auto& is_overloaded = this->is_overloaded;

		//if Op is tok_assign, then it is a special case
		//we do not first do phase1 on LHS here, because it may be a overloaded indexexpr
		//if we do phase1 on overloaded indexexpr, it will create a callexprast to "__getitem__" which is not necessary 
		if (Op == tok_assign)
		{
			auto indexexpr = dyncast_resolve_anno<IndexExprAST>(LHS.get());
			if (indexexpr)
			{
				if (indexexpr->isOverloaded()) //if indexexpr's Expr is a class object, generate "__setitem__"
				{
					auto memberexpr = ResolveAndCreateMemberFunctionExpr(std::move(indexexpr->Expr), "__setitem__", Pos);
					vector<unique_ptr<ExprAST>> args; 
					args.emplace_back(std::move(indexexpr->Index));
					args.emplace_back(std::move(RHS));
					LHS = make_unique<CallExprAST>(std::move(memberexpr), std::move(args));
					LHS->Pos = Pos;
					//Index->Phase1 and RHS->Phase1 will be called in CallExprAST
					LHS->Phase1();
					resolved_type = LHS->resolved_type;
					is_overloaded = true;
					return;
				}
				//else, goto the following control flow
			}
			LHS->Phase1();
			if (!indexexpr) //if LHS is not IndexExprAST
			{
				if (IdentifierExprAST* idexpr = dyncast_resolve_anno<IdentifierExprAST>(LHS.get()))
					CompileAssert(idexpr->impl->isMutable(), Pos, "Cannot assign to an immutable value");
				else if (MemberExprAST* memexpr = dyncast_resolve_anno<MemberExprAST>(LHS.get()))
					CompileAssert(memexpr->isMutable(), Pos, "Cannot assign to an immutable value");
				else
					throw CompileError(Pos, "The left vaule of the assignment is not an variable");
			}
			CompileAssert(LHS->GetLValue(true), Pos, "The left side of = must be an LValue");
			RHS->Phase1();
			RHS = FixTypeForAssignment(LHS->resolved_type, std::move(RHS), Pos);
			resolved_type.type = tok_void;
			return;
		}

		LHS->Phase1();
		//important! call RHS->Phase1()!
		//it is not called here because we may put RHS as a parameter in a function

		auto gen_call_to_operator_func = [&LHS,&RHS,&resolved_type,&Pos,&is_overloaded](const string& name) {
			auto memberexpr = ResolveAndCreateMemberFunctionExpr(std::move(LHS), name, Pos);
			vector<unique_ptr<ExprAST>> args; args.emplace_back(std::move(RHS));
			LHS = make_unique<CallExprAST>(std::move(memberexpr), std::move(args));
			LHS->Pos = Pos;
			//RHS->Phase1 will be called in CallExprAST
			LHS->Phase1();
			is_overloaded = true;
			resolved_type = LHS->resolved_type;
		};
		if (Op == tok_equal || Op == tok_ne)
		{
			if (LHS->resolved_type.type == tok_class && LHS->resolved_type.index_level == 0) //if is class object, check for operator overload
				gen_call_to_operator_func(Op == tok_equal ? "__eq__": "__ne__");
			else
			{
				RHS->Phase1();
				if (LHS->resolved_type == RHS->resolved_type)
					resolved_type.type = tok_boolean;
				else
					resolved_type.type = PromoteNumberExpression(LHS, RHS, true, Pos);
			}
		}
		else if (Op == tok_cmp_equal || Op == tok_cmp_ne)
		{
			RHS->Phase1();
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
				string& name = operator_map.find(Op)->second;
				gen_call_to_operator_func(name);
				return;
			}
			RHS->Phase1();
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
			resolved_type.type = PromoteNumberExpression(LHS, RHS, isBooleanToken(Op), Pos);
		}

	}

	MemberExprAST::MemberExprAST(std::unique_ptr<ExprAST> &&Obj,
		MemberFunctionDef* member, SourcePos pos)
		: Obj(std::move(Obj)), func(member),
		kind(member->virtual_idx== MemberFunctionDef::VIRT_NONE? member_function:member_virtual_function) {
		resolved_type = func->decl->resolved_type;
		Pos = pos;
	}

	void UnaryExprAST::Phase1()
	{
		auto& arg = this->arg;
		auto& Pos = this->Pos;
		auto& resolved_type = this->resolved_type;
		auto& is_overloaded = this->is_overloaded;
		auto TryCreateOverloadedCall = [&](const char* funcname) -> bool {
			arg->Phase1();
			if (arg->resolved_type.type == tok_class && arg->resolved_type.index_level == 0) //if is class object, check for operator overload
			{
				auto memberexpr = ResolveAndCreateMemberFunctionExpr(std::move(arg), funcname, Pos);
				vector<unique_ptr<ExprAST>> args;
				arg = make_unique<CallExprAST>(std::move(memberexpr), std::move(args));
				arg->Pos = Pos;
				arg->Phase1();
				resolved_type = arg->resolved_type;
				is_overloaded = true;
				return true;
			}
			return false;
		};
		switch (Op)
		{
		case tok_not:
		{
			if (!TryCreateOverloadedCall("__not__"))
			{
				resolved_type.type = tok_boolean;
				arg = FixTypeForAssignment(resolved_type, std::move(arg), Pos);
			}
			break;
		}
		case tok_mul:
		{
			if (!TryCreateOverloadedCall("__deref__"))
				throw CompileError(Pos, "The unary operator \"*\" can only be applied on objects");
			break;
		}
		case tok_minus:
		{
			if (!TryCreateOverloadedCall("__neg__"))
			{
				CompileAssert(arg->resolved_type.isNumber(), Pos, "The unary operator \"-\" can only be applied on objects or numbers");
				resolved_type = arg->resolved_type;
			}
			break;
		}
		case tok_pointer_of:
		{
			arg->Phase1();
			CompileAssert(arg->resolved_type.isReference(), Pos, "The expression in pointerof should be a reference type");
			resolved_type.type = tok_pointer;
			break;
		}
		case tok_address_of:
		{
			arg->Phase1();
			CompileAssert(arg->GetLValue(true), Pos, "The expression in addressof should be a LValue");
			resolved_type.type = tok_pointer;
			break;
		}
		case tok_typeof:
		{
			arg->Phase1();
			CompileAssert(arg->resolved_type.type == tok_class
				&& arg->resolved_type.class_ast->needs_rtti && !arg->resolved_type.class_ast->is_struct,
				Pos, "typeof must be appiled on class references with runtime type info");
			resolved_type = ResolvedType(GetTypeInfoClass());
			break;
		}
		default:
			assert(0);
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
		CompileAssert(stmt.size(), Pos, "Cannot get LValue from empty script");
		CompileAssert(instance_of<ExprAST>(stmt.front().get()), Pos, "Getting LValue from statement is illegal");
		auto expr = static_cast<ExprAST*>(stmt.front().get());
		assert(expr->resolved_type.isResolved());
		return expr->GetLValue(checkHas);
	}

	DeferBlockAST::DeferBlockAST(ASTBasicBlock&& defer_block, SourcePos pos) : defer_block(std::move(defer_block)) {
		Pos = pos;
	}
	void DeferBlockAST::Phase1()
	{
		defer_block.Phase1();
	}

	TryBlockAST::TryBlockAST(ASTBasicBlock&& try_block,
		vector<unique_ptr<VariableSingleDefAST>>&& catch_variables,
		vector<ASTBasicBlock>&& catch_blocks, SourcePos pos)
		: try_block(std::move(try_block)), catch_variables(std::move(catch_variables)), catch_blocks(std::move(catch_blocks))
	{
		Pos = pos;
	}

	void TryBlockAST::Phase1()
	{
		try_block.Phase1();
		for (int i = 0; i < catch_variables.size(); i++)
		{
			scope_mgr.PushBasicBlock();
			auto& var = catch_variables[i];
			var->Phase1();
			CompileAssert(var->resolved_type.type == tok_class && var->resolved_type.class_ast->needs_rtti,
				var->Pos, "The exception variable defined in the catch clause must be a class object with rtti enabled");
			catch_blocks[i].Phase1();
			scope_mgr.PopBasicBlock();
		}
	}
}