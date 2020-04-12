#include <BdAST.h>
#include <nlohmann/json.hpp>
#include <nlohmann/fifo_map.hpp>

#include "Metadata.h"
#include <iostream>
#include <ostream>
#include <fstream>
#include <assert.h>
#include <stdlib.h>
#include "Tokenizer.h"
#include "CompileError.h"

using std::unordered_map;
template<class K, class V, class dummy_compare, class A>
using my_workaround_fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;
using json = nlohmann::basic_json<my_workaround_fifo_map>;
using namespace Birdee;

static std::vector<ClassAST*> idx_to_class;
static std::vector<PrototypeAST*> idx_to_proto;
static std::vector<ClassAST*> orphan_idx_to_class;
//the orphan classes that needs to call PreGenerate on
static std::vector<ClassAST*> orphan_class_to_generate;
static std::vector<ClassAST*> to_run_phase0;
static string current_package_name;
static int current_module_idx;
static std::vector<FunctionAST*> annotations_to_run;

extern Tokenizer SwitchTokenizer(Tokenizer&& tokzr);
extern std::unique_ptr<FunctionAST> ParseFunction(ClassAST*, bool is_pure_virtual = false);
extern void ParseClassInPlace(ClassAST* ret, bool is_struct, bool is_interface);

extern std::vector<std::string> Birdee::source_paths;
extern void Birdee_Register_Module(const string& name, void* globals);
extern void PushClass(ClassAST* cls);
extern void PopClass();

namespace Birdee
{
	extern void PushPyScope(ImportedModule* mod);
	extern void PopPyScope();
	extern void GetPyScope(void*& globals, void*& locals);
	extern 	void AddFunctionToClassExtension(FunctionAST* func,
		unordered_map<std::pair<ClassAST*, str_view>, FunctionAST*, pair_hash>& targetmap);
}

static Tokenizer StartParseTemplate(string&& str, Token& first_tok, int line)
{
	Tokenizer toknzr(std::make_unique<StringStream>(std::move(str)), source_paths.size() - 1);
	toknzr.SetLine(line);
	toknzr.GetNextToken();
	first_tok = toknzr.CurTok;
	toknzr.GetNextToken();
	return SwitchTokenizer(std::move(toknzr));
}

template<typename T>
T ReadJSONWithDefault(const json& j, const char* name, T defaults)
{
	auto itr = j.find(name);
	if (itr != j.end())
		return itr->get<T>();
	else
		return defaults;
}

ClassAST* ConvertIdToClass(int idtype)
{
	BirdeeAssert(idtype < MAX_CLASS_COUNT, "Bad type id for class");

	if (idtype < MAX_CLASS_DEF_COUNT)
	{
		BirdeeAssert(idtype < idx_to_class.size(), "Bad type id for class");
		return idx_to_class[idtype];
	}
	else
	{
		idtype -= MAX_CLASS_DEF_COUNT;
		BirdeeAssert(idtype < orphan_idx_to_class.size(), "Bad type id for class");
		auto ret = orphan_idx_to_class[idtype];
		BirdeeAssert(ret, "Bad type id for orphan class - the class is not pre-generated");
		return ret;
	}

}

ResolvedType ConvertIdToType(const json& type)
{
	ResolvedType ret;
	BirdeeAssert(type.type() == json::value_t::object, "Expected an object in JSON");
	long idtype = type["base"].get<long>();
	switch (idtype)
	{
	case -1:
		ret.type = tok_boolean;
		break;
	case -2:
		ret.type = tok_uint;
		break;
	case -3:
		ret.type = tok_int;
		break;
	case -4:
		ret.type = tok_long;
		break;
	case -5:
		ret.type = tok_ulong;
		break;
	case -6:
		ret.type = tok_float;
		break;
	case -7:
		ret.type = tok_double;
		break;
	case -8:
		ret.type = tok_pointer;
		break;
	case -9:
		ret.type = tok_void;
		break;
	case -10:
		ret.type = tok_byte;
		break;
	case -11:
		ret.type = tok_short;
		break;
	default:
		if (idtype < 0)
		{
			BirdeeAssert(idtype <= -MAX_BASIC_TYPE_COUNT && idtype >= -MAX_CLASS_COUNT, "Bad type id");
			ret.type = tok_func;
			BirdeeAssert(-MAX_BASIC_TYPE_COUNT-idtype < idx_to_proto.size(), "Bad type id");
			ret.proto_ast = idx_to_proto[-MAX_BASIC_TYPE_COUNT - idtype];
		}
		else
		{
			ret.class_ast = ConvertIdToClass(idtype);
			ret.type = tok_class;
		}
	}
	ret.index_level = type["index"].get<int>();
	return ret;
}

static AccessModifier GetAccessModifier(const string& acc)
{
	if (acc == "private")
		return access_private;
	else if (acc == "public")
		return access_public;
	BirdeeAssert(0, "Bad access modifier");
	return access_public;
}

unique_ptr<VariableSingleDefAST> BuildVariableFromJson(const json& var)
{
	unique_ptr<VariableSingleDefAST> ret = make_unique<VariableSingleDefAST>(var["name"].get<string>(),
		ConvertIdToType(var["type"]));
	ret->is_threadlocal = ReadJSONWithDefault(var, "threadlocal", false);
	ret->is_volatile = ReadJSONWithDefault(var, "volatile", false);
	return std::move(ret);
}

bool IsSymbolGlobal(const json& itr)
{
	auto mitr = itr.find("is_public");
	if (mitr != itr.end())
	{
		return mitr->get<bool>();
	}
	return false;
}

void BuildGlobalVaribleFromJson(const json& globals, ImportedModule& mod)
{
	BirdeeAssert(globals.is_array(), "Expected a JSON array");
	for (auto& itr : globals)
	{
		auto var = BuildVariableFromJson(itr);
		var->Pos = SourcePos(source_paths.size() - 1, ReadJSONWithDefault(itr, "line", 1), ReadJSONWithDefault(itr, "pos", 1));
		var->PreGenerateExternForGlobal(current_package_name);
		auto& name = var->name;
		mod.dimmap[name] = std::make_pair(std::move(var), IsSymbolGlobal(itr));
	}
}


unique_ptr<FunctionAST> BuildFunctionFromJson(const json& func, ClassAST* cls)
{
	BirdeeAssert(func.is_object(), "Expected a JSON object");
	vector<unique_ptr<VariableSingleDefAST>> Args;

	string name = func["name"];
	const json& args = func["args"];
	BirdeeAssert(args.is_array(), "Expected a JSON array");
	for (auto& arg : args)
	{
		Args.push_back(BuildVariableFromJson(arg));
	}
	auto proto = make_unique<PrototypeAST>(name, std::move(Args), ConvertIdToType(func["return"]), cls, current_module_idx, /*is_closure*/false);
	auto protoptr = proto.get();
	auto ret = make_unique<FunctionAST>(std::move(proto));
	{
		auto itr = func.find("link_name");
		if (itr != func.end())
			ret->link_name = itr->get<string>();
	}
	{
		auto itr = func.find("is_extension");
		if (itr != func.end())
			ret->is_extension = itr->get<bool>();
	}
	ret->Pos = SourcePos(source_paths.size() - 1, ReadJSONWithDefault(func, "line", 1), ReadJSONWithDefault(func, "pos", 1));
	ret->resolved_type.type = tok_func;
	ret->resolved_type.proto_ast = protoptr;
	return std::move(ret);
}

void BuildAnnotationsOnTemplate(const json& json_cls, std::vector<std::string>*& annotation, ImportedModule& mod)
{
	auto anno_itr = json_cls.find("annotations");
	if (anno_itr != json_cls.end())
	{
		BirdeeAssert(anno_itr->is_array(), "The \'annotations\' json field should be an array");
		vector<string> data;
		for (auto& j : *anno_itr)
		{
			data.emplace_back(j.get<string>());
		}
		mod.annotations.push_back(make_unique<AnnotationStatementAST>(std::move(data), nullptr)); //fix-me: assign ret to AnnotationStatementAST?
		annotation = &mod.annotations.back()->anno;
	}
}


void Birdee_RunAnnotationsOn(std::vector<std::string>& anno, StatementAST* ths, SourcePos pos, void* globalscope);

void BuildGlobalFuncFromJson(const json& globals, ImportedModule& mod)
{
	BirdeeAssert(globals.is_array(), "Expected a JSON array");
	for (auto& itr : globals)
	{
		auto var = BuildFunctionFromJson(itr, nullptr);
		BuildAnnotationsOnTemplate(itr, var->annotation, mod);
		if (var->is_extension)
		{
			AddFunctionToClassExtension(var.get(), mod.class_extend_funcmap);
		}
		var->PreGenerate();
		if (var->annotation)
		{
			annotations_to_run.push_back(var.get());
		}
		auto name = var->Proto->GetName();
		mod.funcmap[name] = std::make_pair(std::move(var), IsSymbolGlobal(itr));
	}
}

static int GetTemplateLineNumber(const json& json_cls)
{
	auto itr = json_cls.find("source_line");
	if (itr != json_cls.end())
	{
		return itr->get<int>();
	}
	return 1;
}

void BuildGlobalTemplateFuncFromJson(const json& globals, ImportedModule& mod)
{
	BirdeeAssert(globals.is_array(), "Expected a JSON array");
	for (auto& itr : globals)
	{
		auto strsrc = itr["template"];
		Token first_tok;
		auto var = StartParseTemplate(strsrc.get<string>(), first_tok, GetTemplateLineNumber(itr));
		BirdeeAssert(tok_func == first_tok , "The first token of template should be function");
		auto func = ParseFunction(nullptr);
		BirdeeAssert(func->template_param.get(), "func->template_param");
		func->template_param->mod = &mod;
		func->template_param->source.set(strsrc.get<string>());
		SwitchTokenizer(std::move(var));
		cu.imported_func_templates.push_back(func.get());
		func->Proto->prefix_idx = current_module_idx;
		BuildAnnotationsOnTemplate(itr, func->annotation, mod);
		auto name = func->Proto->GetName();
		mod.funcmap[name] = std::make_pair(std::move(func), IsSymbolGlobal(itr));;
	}
}

void BuildTemplateClassFromJson(const json& itr, ClassAST* cls, int module_idx, int linenumber, ImportedModule& mod)
{
	Token first_tok;
	auto var = StartParseTemplate(itr.get<string>(), first_tok, linenumber);
	BirdeeAssert(tok_class == first_tok || tok_struct == first_tok || tok_interface == first_tok, "The first token of template should be class/struct/interface");
	ParseClassInPlace(cls, tok_struct == first_tok, tok_interface == first_tok);
	BirdeeAssert(cls->template_param.get(), "cls->template_param");
	cls->template_param->mod = &mod;
	cls->template_param->source.set(itr.get<string>());
	SwitchTokenizer(std::move(var));
	cu.imported_class_templates.push_back(cls);
	cls->package_name_idx = module_idx;
}

unique_ptr<vector<TemplateArgument>> BuildTemplateArgsFromJson(const json& args)
{
	auto targs = make_unique< vector<TemplateArgument>>(); //GetOrCreate will take its ownership
	for (auto& arg : args)
	{
		if (arg["kind"].get<string>() == "type")
		{
			targs->push_back(TemplateArgument(ConvertIdToType(arg["type"])));
		}
		else if (arg["kind"].get<string>() == "expr")
		{
			string exprtype = arg["exprtype"].get<string>();
			if (exprtype == "string")
			{
				targs->push_back(TemplateArgument(std::make_unique<StringLiteralAST>(arg["value"].get<string>())));
			}
			else
			{
				auto tok_itr = Tokenizer::token_map.find(exprtype);
				BirdeeAssert(tok_itr != Tokenizer::token_map.end(), "Unknown exprtype");
				//fix-me: check the token type. is it a number?
				std::stringstream buf(arg["value"].get<string>());
				NumberLiteral nliteral;
				nliteral.type = tok_itr->second;
				buf >> nliteral.v_ulong;
				targs->push_back(TemplateArgument(std::make_unique <NumberExprAST>(nliteral)));
			}
		}
		else
		{
			BirdeeAssert(false, "Bad template argument kind");
		}
	}
	return std::move(targs);
}

void BuildSingleClassFromJson(ClassAST* ret, const json& json_cls, int module_idx, ImportedModule& mod)
{
	auto rtti_itr = json_cls.find("needs_rtti");
	if (rtti_itr != json_cls.end())
		ret->needs_rtti = rtti_itr->get<bool>();
	BuildAnnotationsOnTemplate(json_cls, ret->annotation, mod);
	auto temp_itr = json_cls.find("template");
	if (temp_itr != json_cls.end()) //if it's a template
	{
		BuildTemplateClassFromJson(*temp_itr, ret, module_idx, GetTemplateLineNumber(json_cls), mod);
		BirdeeAssert(ret->isTemplate(), "The class with \'template\' json field should be a template");
		return;
	}
	{
		auto itr = json_cls.find("parent");
		if (itr != json_cls.end())
		{
			ret->parent_class = ConvertIdToClass(itr->get<int>());
		}
			
	}
	ret->name = json_cls["name"].get<string>();
	ret->package_name_idx = module_idx;
	{
		auto itr = json_cls.find("is_interface");
		if (itr != json_cls.end())
			ret->is_interface = itr->get<bool>();

		itr = json_cls.find("implements");
		if (itr != json_cls.end()) {
			const json& json_impls = *itr;
			BirdeeAssert(json_impls.is_array(), "Expected an JSON array");
			for (auto& impl : json_impls)
			{
				ret->implements.push_back(ConvertIdToClass(impl.get<int>()));
			}
		}
	}

	{
		auto itr = json_cls.find("is_struct");
		if(itr!=json_cls.end())
			ret->is_struct = itr->get<bool>();
	}
	const json& json_fields = json_cls["fields"];
	BirdeeAssert(json_fields.is_array(), "Expected an JSON array");
	int idx = 0;
	for (auto& field : json_fields)
	{
		AccessModifier acc = GetAccessModifier(field["access"].get<string>());
		auto var = BuildVariableFromJson(field["def"]);
		string& str = var->name;
		ret->fields.push_back(FieldDef(acc, std::move(var), idx));
		ret->fieldmap[str] = idx;
		idx++;
	}

	idx = 0;
	const json& json_funcs = json_cls["funcs"];
	BirdeeAssert(json_funcs.is_array(), "Expected an JSON array");
	bool is_virtual = false;

	if (ret->template_instance_args == nullptr) //if we have already put the argument, we can skip
	{
		auto templ_args = json_cls.find("template_arguments");
		if (templ_args != json_cls.end())
		{
			ret->template_instance_args = unique_ptr<vector<TemplateArgument>>(BuildTemplateArgsFromJson(*templ_args));
		}
	}

	for (auto& func : json_funcs)
	{
		json json_func;
		AccessModifier acc = GetAccessModifier(func["access"].get<string>());
		unique_ptr<FunctionAST> funcdef;
		auto templ = func.find("template");
		if (templ != func.end())
		{
			Token first_tok;
			Tokenizer old = StartParseTemplate(templ->get<string>(), first_tok, GetTemplateLineNumber(func));
			BirdeeAssert(tok_func == first_tok, "The first token of template should be function");
			funcdef = ParseFunction(ret);
			BuildAnnotationsOnTemplate(func, funcdef->annotation, mod);
			BirdeeAssert(funcdef->template_param.get(), "func->template_param");
			funcdef->template_param->mod = &mod;
			funcdef->template_param->source.set(templ->get<string>());
			//if the class is a template instance, the function will be generated via class_template->Generate()
			//so we don't need to put it into imported_func_templates
			if (!ret->isTemplateInstance()) 
				cu.imported_func_templates.push_back(funcdef.get());
			funcdef->Proto->prefix_idx = current_module_idx;
			SwitchTokenizer(std::move(old));
		}
		else
		{
			funcdef = BuildFunctionFromJson(func["def"], ret);
			BuildAnnotationsOnTemplate(func["def"], funcdef->annotation, mod);
			if (funcdef->annotation)
				annotations_to_run.push_back(funcdef.get());
		}
		const string& str = funcdef->Proto->GetName();
		auto vidx_itr = func.find("virtual_idx");
		ret->funcs.push_back(MemberFunctionDef(acc, std::move(funcdef), std::vector<std::string>(), vidx_itr != func.end() ? vidx_itr->get<int>() : MemberFunctionDef::VIRT_NONE));
		is_virtual |= (ret->funcs.back().virtual_idx != MemberFunctionDef::VIRT_NONE);
		ret->funcmap[str] = idx;
		idx++;
	}
	if (is_virtual)
		to_run_phase0.push_back(ret);
	else
		ret->done_phase = 1;
}

void PreBuildClassFromJson(const json& cls, const string& module_name, ImportedModule& mod)
{
	BirdeeAssert(cls.is_array(), "Expeccted a JSON array");
	for (auto& json_cls : cls)
	{
		auto itr = json_cls.find("name");
		if (itr != json_cls.end()) //if it's a class def or class template
		{
			//BirdeeAssert(!has_end, "Class def is not allowed after class template instances");
			string name = itr->get<string>();
			unique_ptr<ClassAST> classdef;
			int src_line = ReadJSONWithDefault(json_cls, "line", 0);
			int src_pos = ReadJSONWithDefault(json_cls, "pos", 0);
			classdef = make_unique<ClassAST>(string(), SourcePos(source_paths.size() - 1, src_line, src_pos)); //add placeholder
			
			idx_to_class.push_back(classdef.get());
			mod.classmap[name] = std::make_pair(std::move(classdef), IsSymbolGlobal(json_cls));
		}
		else//assert that it's a class template instance
		{
			BirdeeAssert(false, "The class needs a name");
		}

	}
}



void PreBuildOneOrphanClassFromJson(const json& cls, int idx,
	const unordered_map<string, std::pair<ImportedModule*, string>>& clsname2mod)
{
	if (orphan_idx_to_class[idx])
		return;

	auto& json_cls = cls[idx];
	string name = json_cls["name"].get<string>();
	ClassAST* classdef;
	//whether we need to call PreGenerate on this class?
	bool needs_generate = true;
	if (json_cls.size() == 1) // if we have only one field, this class is redirected to another module
	{
		auto moddef_itr = clsname2mod.find(name);
		BirdeeAssert(moddef_itr != clsname2mod.end(), "Cannot find the pre-imported module");
		auto src_mod = moddef_itr->second.first;
		auto& cls_simple_name = moddef_itr->second.second;
		auto clsdef_itr = src_mod->classmap.find(cls_simple_name);
		BirdeeAssert(clsdef_itr != src_mod->classmap.end(), "Cannot find the class in pre-imported module");
		classdef = clsdef_itr->second.first.get();
		needs_generate = false;
	}
	else
	{
		//find if already defined by imported modules

		//first, check if it is a template instance
		auto templ_idx = name.find('[');
		BirdeeAssert(templ_idx != string::npos, "Invalid imported class name, must be a template instance");
			
		{//template instance
			auto idx = name.find_last_of('.', templ_idx - 1);
			BirdeeAssert(idx != string::npos && idx != templ_idx - 1, "Invalid imported class name");
			auto node = cu.imported_packages.Contains(name, idx);
			if (node)
			{//if the package is imported
				BirdeeAssert(templ_idx > idx, "Invalid template instance class name");
				auto itr = node->mod->classmap.find(name.substr(idx + 1, templ_idx - 1 - idx));
				BirdeeAssert(itr != node->mod->classmap.end(), "Module imported, but cannot find the class");
				auto src = itr->second.first.get();
				BirdeeAssert(src->isTemplate(), "The source must be a template");
				//now make sure all template arguments are pre-built
				auto& targs = json_cls["template_arguments"];
				for (auto& arg : targs)
				{
					if (arg["kind"].get<string>() == "type")
					{
						auto& type = arg["type"];
						BirdeeAssert(type.type() == json::value_t::object, "Expected an object in JSON");
						long idtype = type["base"].get<long>();
						if (idtype >= MAX_CLASS_DEF_COUNT) // if it is an orphan class, pre-build it
							PreBuildOneOrphanClassFromJson(cls, idtype - MAX_CLASS_DEF_COUNT, clsname2mod);
					}
				}

				//add to the existing template's instances
				auto pargs = BuildTemplateArgsFromJson(targs);
				classdef = src->template_param->Get(*pargs); //find if we have an instance?
				if (!classdef)
				{ //if have an instance, do nothing
				  //if no instance, add a placeholder to the instance set
					auto newclass = make_unique<ClassAST>(string(), SourcePos(source_paths.size() - 1, 0, 0)); //placeholder
					classdef = newclass.get();
					auto& vec = *pargs;
					classdef->template_instance_args = std::move(pargs);
					classdef->template_source_class = src;
					src->template_param->AddImpl(vec, std::move(newclass));
				}
				else
				{
					needs_generate = false;
				}
			}
			else
			{//if the template itself is not imported, raise an error
				BirdeeAssert(false, "Cannot import a class template because cannot find the source module");
			}
		}
	}
	orphan_idx_to_class[idx] = classdef;
	if (needs_generate)
		orphan_class_to_generate.push_back(classdef);
}

void PreBuildOrphanClassFromJson(const json& cls, 
	const unordered_map<string, std::pair<ImportedModule*, string>>& clsname2mod,ImportedModule& mod)
{
	BirdeeAssert(cls.is_array(), "Expeccted a JSON array");
	orphan_idx_to_class.resize(cls.size(), nullptr);
	for (int idx = cls.size() - 1; idx >=0; idx--) // reversed order to make sure all dependency has been pre-built
	{
		PreBuildOneOrphanClassFromJson(cls, idx, clsname2mod);
	}
}

void PreBuildPrototypeFromJson(const json& funcs, ImportedModule& mod)
{
	BirdeeAssert(funcs.is_array(), "Expeccted a JSON array");
	//we use a two-pass approach: first pass, add placeholder
	//second pass, put real data into placeholder
	//Why bother the two-pass approach? func_ty0 may reference func_ty1. This may cause some problems.
	int idx = 0;
	for (auto& func : funcs)
	{
		BirdeeAssert(func.is_object(), "Expected a JSON object");
		string name = func["name"];
		bool is_closure = func["is_closure"].get<bool>();
		auto proto = make_unique<PrototypeAST>("", vector<unique_ptr<VariableSingleDefAST>>(),
			ResolvedType(), (ClassAST*)nullptr, current_module_idx, is_closure);
		proto->pos = SourcePos(source_paths.size() - 1, ReadJSONWithDefault(func, "line", 1), ReadJSONWithDefault(func, "pos", 1));
		auto protoptr = proto.get();
		if (name.size() != 0) //if it is a named prototype
			mod.functypemap[name] = std::make_pair(std::move(proto), IsSymbolGlobal(func));
		else
		{
			std::stringstream strbuf;
			strbuf << idx; //just put a number as the unique name
			mod.functypemap[strbuf.str()] = std::make_pair(std::move(proto), IsSymbolGlobal(func));
		}
		idx_to_proto.push_back(protoptr);
		idx++;
	}
}

void BuildPrototypeFromJson(const json& funcs, ImportedModule& mod)
{
	BirdeeAssert(funcs.is_array(), "Expeccted a JSON array");
	//we use a two-pass approach: first pass, add placeholder
	//second pass, put real data into placeholder
	//Why bother the two-pass approach? func_ty0 may reference func_ty1. This may cause some problems.
	int idx = 0;
	for (auto& func : funcs)
	{
		auto proto = idx_to_proto[idx];
		vector<unique_ptr<VariableSingleDefAST>> Args;

		const json& args = func["args"];
		auto itr = func.find("class");
		if (itr != func.end())
		{
			auto clstype = ConvertIdToType(*itr);
			BirdeeAssert(clstype.type==tok_class, "Expected a class");
			proto->cls = clstype.class_ast;
		}
		else
		{
			proto->cls = nullptr;
		}
		BirdeeAssert(args.is_array(), "Expected a JSON array");
		BirdeeAssert(proto->resolved_args.empty(), "proto->resolved_args.empty()");
		for (auto& arg : args)
		{
			proto->resolved_args.push_back(BuildVariableFromJson(arg));
		}

		proto->resolved_type = ConvertIdToType(func["return"]);
		idx++;
	}
}

void BuildClassFromJson(const json& cls, ImportedModule& mod)
{
	BirdeeAssert(cls.is_array(), "Expeccted a JSON array");
	auto itr = idx_to_class.begin();
	for (auto& json_cls : cls)
	{

		if ((*itr)->name.size() == 0) // if it is a placeholder
		{
			BuildSingleClassFromJson((*itr), json_cls, current_module_idx, mod);
		}
		//fix-me: check if the imported type is the same as the existing type


		auto srcitr = json_cls.find("template_source");
		if (srcitr != json_cls.end()) //if it's a class template instance
		{
			int src_idx = srcitr->get<int>();
			BirdeeAssert(src_idx >= 0 && src_idx < idx_to_class.size(), "Template source index out of index");
			ClassAST* src = idx_to_class[src_idx];
			BirdeeAssert(src->isTemplate(), "The source must be a class template");
			auto targs = BuildTemplateArgsFromJson(json_cls["template_arguments"]);
			auto name_cls_itr = mod.classmap.find((*itr)->name);
			assert(name_cls_itr != mod.classmap.end());
			auto oldcls = src->template_param->Get(*targs);
			if (!oldcls)
			{
				src->template_param->AddImpl(*targs, std::move(name_cls_itr->second.first));
				(*itr)->template_instance_args = std::move(targs);
				(*itr)->template_source_class = src;
			}
			mod.classmap.erase(name_cls_itr);
		}
		itr++;
	}
}

void BuildOrphanClassFromJson(const json& cls, ImportedModule& mod)
{
	BirdeeAssert(cls.is_array(), "Expeccted a JSON array");
	auto itr = orphan_idx_to_class.begin();
	for (auto& json_cls : cls)
	{
		if ((*itr)->name.size() == 0) // if it is a placeholder
		{
			BuildSingleClassFromJson((*itr), json_cls, -2, mod); //orphan classes, module index=-2
		}
		itr++;
	}
}

extern string GetModuleNameByArray(const vector<string>& package, const char* delimiter = ".");

typedef string(*ModuleResolveFunc)(const vector<string>& package, unique_ptr<std::istream>& f, bool second_chance);
static ModuleResolveFunc module_resolver = nullptr;
BD_CORE_API void SetModuleResolver(ModuleResolveFunc f)
{
	module_resolver = f;
}

static string GetModuleFile(const vector<string>& package, unique_ptr<std::istream>& f)
{
	string ret;
	if (module_resolver)
	{
		ret = module_resolver(package, f, false);
		if (!ret.empty())
			return ret;
	}
	string libpath;
	for (auto& s : package)
	{
		libpath += '/';
		libpath += s;
	}
	libpath += ".bmm";

	for (auto& spath : cu.search_path)
	{
		ret = spath + libpath;
		auto of = std::make_unique<std::ifstream>(ret, std::ios::in);
		if (*of)
		{
			f = std::move(of);
			return ret;
		}
	}
	
	string cur_dir_path = cu.directory + "/";
	auto dot_cnt=std::count(cu.symbol_prefix.begin(), cu.symbol_prefix.end(), '.');
	if (dot_cnt > 0)
	{
		for (int i = 0; i < dot_cnt - 1; i++)
			cur_dir_path += "../";
		ret = cur_dir_path + libpath;
		auto of = std::make_unique<std::ifstream>(ret, std::ios::in);
		if (*of)
		{
			f = std::move(of);
			return ret;
		}
	}

	ret = cu.homepath + "blib" + libpath;
	auto of = std::make_unique<std::ifstream>(ret, std::ios::in);
	if (*of)
	{
		f = std::move(of);
		return ret;
	}

	if (module_resolver)
	{
		ret = module_resolver(package, f, true);
		if (!ret.empty())
			return ret;
	}

	return "";

}


void Get2DStringArray(vector<vector<string>>& ret, const json& js)
{
	for (auto& arr : js)
	{
		ret.push_back(vector<string>());
		for (auto& itm : arr)
		{
			ret.back().push_back(itm.get<string>());
		}
	}
}

extern void SplitString(const string& str, char delimiter, vector<string>& ret);
extern ImportedModule* DoImportPackage(const vector<string>& package);

// import the modules that "ImportedClasses" needs, returns a map from qualified class name to (ImportedModule, simple class name)
static void ImportDependencies(const json& indata, unordered_map<string, std::pair<ImportedModule*, string>>& clsname2mod)
{
	const auto& cls = indata["ImportedClasses"];
	for (auto itr : cls)
	{
		// for each imported class, find the classes with only "name" field
		if (itr.size() == 1)
		{
			auto itrname = itr.find("name");
			if (itrname != itr.end())
			{
				vector<string> name;
				SplitString(itrname->get<string>(), '.', name);
				BirdeeAssert(name.size() > 1, "Bad imported class name");
				string cls_name = std::move(name.back());
				name.pop_back();
				clsname2mod.insert(std::make_pair(
					itrname->get<string>(),  std::make_pair(DoImportPackage(name), std::move(cls_name)))
					);
			}
		}
	}
}

void ImportedModule::Init(const vector<string>& package, const string& module_name)
{
	json json;
	
	unique_ptr<std::istream> ptrin; 
	string path = GetModuleFile(package, ptrin);
	auto& in = *ptrin;
	if (!ptrin || !in)
	{
		throw CompileError(string("Cannot find module ") + GetModuleNameByArray(package));
	}
	in >> json;
	int tmp_current_module_idx = cu.imported_module_names.size() - 1;
	unordered_map<string, std::pair<ImportedModule*, string>> clsname2mod;
	ImportDependencies(json, clsname2mod);

	current_package_name = module_name;
	current_module_idx = tmp_current_module_idx;
	idx_to_proto.clear();
	idx_to_class.clear();
	to_run_phase0.clear();
	annotations_to_run.clear();
	orphan_idx_to_class.clear();
	orphan_class_to_generate.clear();
	string srcpath;
	{
		auto itr = json.find("SourceFile");
		if (itr != json.end())
			srcpath = itr->at(0).get<string>() + "/" + itr->at(1).get<string>();
		else
			srcpath = path;
	}
	source_paths.push_back(srcpath);
	BirdeeAssert(json["Type"].get<string>() == "Birdee Module Metadata", "Bad file type");
	BirdeeAssert(json["Version"].get<double>() <= META_DATA_VERSION, "Unsupported version");
	BirdeeAssert(json["Package"] == module_name, "The module path does not fit the package name");

	struct RTTIPyScope
	{
		bool need=false;
		~RTTIPyScope() {
			if (need)
			{
				Birdee::PopPyScope();
			}
		}
	}pyscope;
	auto scriptitr = json.find("InitScripts");
	if (scriptitr != json.end())
	{
		BirdeeAssert(scriptitr->is_string(), "InitScripts field in bmm file should be a string");
		//if it has a init script, construct a temp ScriptAST and run it
		Birdee::PushPyScope(this);
		pyscope.need = true;
		ScriptAST tmp = ScriptAST(string(), true);
		tmp.script = scriptitr->get<string>();
		if (!tmp.script.empty())
		{
			tmp.Phase1();
		}
		void* locals, *globals;
		GetPyScope(globals, locals);
		Birdee_Register_Module(GetModuleNameByArray(package, "_0"), globals);
	}
	

	{
		auto itr = json.find("FunctionTemplates");
		if (itr != json.end())
			BuildGlobalTemplateFuncFromJson(*itr, *this);
	}
	{
		auto itr = json.find("HeaderOnly");
		if (itr != json.end())
			is_header_only=itr->get<bool>();
	}

	auto ftyitr = json.find("FunctionTypes");
	if (ftyitr != json.end())
		PreBuildPrototypeFromJson(*ftyitr, *this);
	//we first make place holder for each class. Because classes may reference each other
	PreBuildClassFromJson(json["Classes"], module_name, *this);
	PreBuildOrphanClassFromJson(json["ImportedClasses"], clsname2mod, *this);
	//we then create the classes and prototypes
	if (ftyitr != json.end())
		BuildPrototypeFromJson(*ftyitr,*this);
	BuildClassFromJson(json["Classes"], *this);
	BuildOrphanClassFromJson(json["ImportedClasses"], *this);

	if (package.size() == 1 && package[0] == "birdee") //if is "birdee" core package, generate "type_info" first
		classmap.find("type_info")->second.first->PreGenerate();
	for (auto cls : to_run_phase0)
		cls->Phase0();
	for (auto& cls : this->classmap)
	{
		cls.second.first->PreGenerate();//it is safe to call multiple times
		cls.second.first->PreGenerateFuncs();
		//cls.second->Generate();
	}
	for (auto cls : orphan_class_to_generate)
	{
		cls->PreGenerate();
		cls->PreGenerateFuncs();
		//cls->Generate();
	}

	auto imports_itr = json.find("Imports");
	if (imports_itr != json.end())
		Get2DStringArray(this->user_imports, *imports_itr);

	BuildGlobalVaribleFromJson(json["Variables"], *this);
	BuildGlobalFuncFromJson(json["Functions"], *this);
	
	{
		auto itr = json.find("SourceFile");
		if (itr != json.end())
		{
			auto& arr = *itr;
			BirdeeAssert(arr.is_array() && arr.size()==2, "SourceFile field in bmm file should be an array of 2 elements");
			source_dir = arr[0].get<string>();
			source_file = arr[1].get<string>();
		}
	}

	for (auto funcdef : annotations_to_run)
	{
		if (funcdef->Proto->cls)
			PushClass(funcdef->Proto->cls);
		if (funcdef->annotation)
		{
			void* globals, *locals;
			GetPyScope(globals, locals);
			Birdee_RunAnnotationsOn(*funcdef->annotation, funcdef, funcdef->Pos, globals);
		}
		if (funcdef->Proto->cls)
			PopClass();
	}
}
