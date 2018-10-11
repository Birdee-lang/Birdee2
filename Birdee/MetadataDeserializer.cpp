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

using std::unordered_map;
template<class K, class V, class dummy_compare, class A>
using my_workaround_fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;
using json = nlohmann::basic_json<my_workaround_fifo_map>;
using namespace Birdee;

static std::vector<ClassAST*> idx_to_class;
static std::vector<ClassAST*> orphan_idx_to_class;
static string current_package_name;
static int current_module_idx;

extern Tokenizer SwitchTokenizer(Tokenizer&& tokzr);
extern std::unique_ptr<ClassAST> ParseClass();
extern std::unique_ptr<FunctionAST> ParseFunction(ClassAST*);
extern void ParseClassInPlace(ClassAST* ret);

extern std::vector<std::string> Birdee::source_paths;

/*
fix-me: Load template class & functions & instances
link templates to instances by name (for every new template source imported, remember find instances in orphans)
link instances to templates sources by name (remember find them in orphans)
<del>Template class' template functions are not included in the metadata of class template instances, which are already included in the class template.
<del>remember to restore the functions in the cls template instances!
test if serializing imported template instance classes really works!!!!!!!!!!!! (check if function templates in the template instances are properly dumped)
import template class instance
*/


static Tokenizer StartParseTemplate(string&& str, Token first_tok)
{
	Tokenizer toknzr(std::make_unique<StringStream>(std::move(str)), source_paths.size() - 1);
	toknzr.GetNextToken();
	BirdeeAssert(toknzr.CurTok == first_tok, "The first token of template should be function");
	toknzr.GetNextToken();
	return SwitchTokenizer(std::move(toknzr));
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
	default:
		BirdeeAssert(idtype >= 0 && idtype < MAX_CLASS_COUNT, "Bad type id");
		ret.type = tok_class;
		if (idtype < MAX_CLASS_DEF_COUNT)
		{
			BirdeeAssert(idtype < idx_to_class.size(), "Bad type id");
			ret.class_ast = idx_to_class[idtype];
		}
		else
		{
			idtype -= MAX_CLASS_DEF_COUNT;
			BirdeeAssert(idtype < orphan_idx_to_class.size(), "Bad type id");
			ret.class_ast = orphan_idx_to_class[idtype];
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
	return std::move(ret);
}

void BuildGlobalVaribleFromJson(const json& globals, ImportedModule& mod)
{
	BirdeeAssert(globals.is_array(), "Expected a JSON array");
	for (auto& itr : globals)
	{
		auto var = BuildVariableFromJson(itr);
		var->PreGenerateExternForGlobal(current_package_name);
		mod.dimmap[var->name] = std::move(var);
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
	auto proto = make_unique<PrototypeAST>(name, std::move(Args), ConvertIdToType(func["return"]), cls, current_module_idx);
	auto protoptr = proto.get();
	auto ret = make_unique<FunctionAST>(std::move(proto));
	ret->resolved_type.type = tok_func;
	ret->resolved_type.proto_ast = protoptr;
	return std::move(ret);
}

void BuildGlobalFuncFromJson(const json& globals, ImportedModule& mod)
{
	BirdeeAssert(globals.is_array(), "Expected a JSON array");
	for (auto& itr : globals)
	{
		auto var = BuildFunctionFromJson(itr, nullptr);
		var->PreGenerate();
		mod.funcmap[var->Proto->GetName()] = std::move(var);
	}
}

void BuildGlobalTemplateFuncFromJson(const json& globals, ImportedModule& mod)
{
	BirdeeAssert(globals.is_array(), "Expected a JSON array");
	for (auto& itr : globals)
	{
		auto var = StartParseTemplate(itr.get<string>(), tok_func);
		auto func = ParseFunction(nullptr);
		BirdeeAssert(func->template_param.get(), "func->template_param");
		func->template_param->mod = &mod;
		func->template_param->source.set(itr.get<string>());
		SwitchTokenizer(std::move(var));
		cu.imported_func_templates.push_back(func.get());
		func->Proto->prefix_idx = current_module_idx;
		mod.funcmap[func->Proto->GetName()] = std::move(func);
	}
}

void BuildTemplateClassFromJson(const json& itr, ClassAST* cls, int module_idx, ImportedModule& mod)
{

	auto var = StartParseTemplate(itr.get<string>(), tok_class);
	ParseClassInPlace(cls);
	BirdeeAssert(cls->template_param.get(), "cls->template_param");
	cls->template_param->mod = &mod;
	cls->template_param->source.set(itr.get<string>());
	SwitchTokenizer(std::move(var));
	cu.imported_class_templates.push_back(cls);
	cls->package_name_idx = module_idx;

}

vector<TemplateArgument>* BuildTemplateArgsFromJson(const json& args)
{
	vector<TemplateArgument>* targs = new vector<TemplateArgument>(); //GetOrCreate will take its ownership
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
	return targs;
}

void BuildSingleClassFromJson(ClassAST* ret, const json& json_cls, int module_idx, ImportedModule& mod)
{
	auto temp_itr = json_cls.find("template");
	if (temp_itr != json_cls.end()) //if it's a template
	{
		BuildTemplateClassFromJson(*temp_itr, ret, module_idx, mod);
		BirdeeAssert(ret->isTemplate(), "The class with \'template\' json field should be a template");
		//find the template instances in orphan classes whose name starts with: package.clazzname[......
		string starts = current_package_name + '.' + ret->name + '[';
		for (auto itr = cu.orphan_class.lower_bound(starts); itr != cu.orphan_class.end();)
		{
			if (itr->first.find(starts) != 0)
				break;
			assert(!itr->second->template_source_class);
			//link the instance with the template
			itr->second->template_source_class = ret;
			auto v = itr->second->template_instance_args.get();
			ret->template_param->AddImpl(*v, std::move(itr->second));
			itr = cu.orphan_class.erase(itr);
		}

		return;
	}
	ret->name = json_cls["name"].get<string>();
	ret->package_name_idx = module_idx;
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
	for (auto& func : json_funcs)
	{
		json json_func;
		AccessModifier acc = GetAccessModifier(func["access"].get<string>());
		unique_ptr<FunctionAST> funcdef;
		auto templ = func.find("template");
		if (templ != func.end())
		{
			Tokenizer old = StartParseTemplate(templ->get<string>(), tok_func);
			funcdef = ParseFunction(ret);
			BirdeeAssert(funcdef->template_param.get(), "func->template_param");
			funcdef->template_param->mod = &mod;
			funcdef->template_param->source.set(templ->get<string>());
			cu.imported_func_templates.push_back(funcdef.get());
			funcdef->Proto->prefix_idx = current_module_idx;
			SwitchTokenizer(std::move(old));
		}
		else
		{
			funcdef = BuildFunctionFromJson(func["def"], ret);
		}
		const string& str = funcdef->Proto->GetName();
		ret->funcs.push_back(MemberFunctionDef(acc, std::move(funcdef)));
		ret->funcmap[str] = idx;
		idx++;
	}
	if (ret->template_instance_args == nullptr) //if we have already put the argument, we can skip
	{
		auto templ_args = json_cls.find("template_arguments");
		if (templ_args != json_cls.end())
		{
			ret->template_instance_args = unique_ptr<vector<TemplateArgument>>(BuildTemplateArgsFromJson(*templ_args));
		}
	}
}

void PreBuildClassFromJson(const json& cls, const string& module_name, ImportedModule& mod)
{
	BirdeeAssert(cls.is_array(), "Expeccted a JSON array");
	bool has_end = false;
	for (auto& json_cls : cls)
	{
		auto itr = json_cls.find("name");
		if (itr != json_cls.end()) //if it's a class def or class template
		{
			BirdeeAssert(!has_end, "Class def is not allowed after class template instances");
			string name = itr->get<string>();
			auto orphan = cu.orphan_class.find(module_name + '.' + name);
			unique_ptr<ClassAST> classdef;
			if (orphan != cu.orphan_class.end())
			{
				classdef = std::move(orphan->second);
				cu.orphan_class.erase(orphan);
			}
			else
			{
				classdef = make_unique<ClassAST>(string(), SourcePos(source_paths.size() - 1, 0, 0)); //add placeholder
			}
			idx_to_class.push_back(classdef.get());
			mod.classmap[name] = std::move(classdef);
		}
		else//assert that it's a class template instance
		{
			BirdeeAssert(json_cls.find("source") != json_cls.end(), "Either class def or class template instance is allowed here.");
			has_end = true;
			idx_to_class.push_back(nullptr);
		}

	}
}



void PreBuildOrphanClassFromJson(const json& cls, ImportedModule& mod)
{
	BirdeeAssert(cls.is_array(), "Expeccted a JSON array");
	for (auto& json_cls : cls)
	{
		string name = json_cls["name"].get<string>();
		auto orphan = cu.orphan_class.find(name);
		ClassAST* classdef;
		if (orphan != cu.orphan_class.end())
		{//if already imported in orphan classes
			classdef = orphan->second.get();
		}
		else
		{
			//find if already defined by imported modules

			//first, check if it is a template instance
			auto templ_idx = name.find('[');
			if (templ_idx != string::npos)
			{//template instance
				auto idx = name.find_last_of('.', templ_idx - 1);
				BirdeeAssert(idx != string::npos && idx != templ_idx - 1, "Invalid imported class name");
				auto node = cu.imported_packages.Contains(name, idx);
				if (node)
				{//if the package is imported
					BirdeeAssert(templ_idx > idx, "Invalid template instance class name");
					auto itr = node->mod->classmap.find(name.substr(idx + 1, templ_idx - 1 - idx));
					BirdeeAssert(itr != node->mod->classmap.end(), "Module imported, but cannot find the class");
					auto src = itr->second.get();
					BirdeeAssert(src->isTemplate(), "The source must be a template");
					//add to the existing template's instances
					auto pargs = BuildTemplateArgsFromJson(json_cls["template_arguments"]);
					classdef = src->template_param->Get(*pargs); //find if we have an instance?
					if (classdef)
					{ //if have an instance, do nothing
						delete pargs;
					}
					else
					{ //if no instance, add a placeholder to the instance set
						auto newclass = make_unique<ClassAST>(string(), SourcePos(source_paths.size() - 1, 0, 0)); //placeholder
						classdef = newclass.get();
						classdef->template_instance_args = unique_ptr<vector<TemplateArgument>>(pargs);
						src->template_param->AddImpl(*pargs, std::move(newclass));
					}
				}
				else
				{//if the template itself is not imported, make it an orphan
					auto newclass = make_unique<ClassAST>(string(), SourcePos(source_paths.size() - 1, 0, 0)); //add placeholder
					classdef = newclass.get();
					cu.orphan_class[name] = std::move(newclass);
				}
			}
			else //if it's a class def
			{
				auto idx = name.find_last_of('.');
				BirdeeAssert(idx != string::npos && idx != name.size() - 1, "Invalid imported class name");

				auto node = cu.imported_packages.Contains(name, idx);
				if (node)
				{//if the package is imported
					auto itr = node->mod->classmap.find(name.substr(idx + 1));
					BirdeeAssert(itr != node->mod->classmap.end(), "Module imported, but cannot find the class");
					classdef = itr->second.get();
				}
				else
				{
					auto newclass = make_unique<ClassAST>(string(), SourcePos(source_paths.size() - 1, 0, 0)); //add placeholder
					classdef = newclass.get();
					cu.orphan_class[name] = std::move(newclass);
				}
			}

		}
		orphan_idx_to_class.push_back(classdef);
	}
}



void BuildClassFromJson(const json& cls, ImportedModule& mod)
{
	BirdeeAssert(cls.is_array(), "Expeccted a JSON array");
	auto itr = idx_to_class.begin();
	for (auto& json_cls : cls)
	{
		auto nameitr = json_cls.find("name");
		if (nameitr != json_cls.end()) //if it's a class def or class template
		{
			if ((*itr)->name.size() == 0) // if it is a placeholder
			{
				BuildSingleClassFromJson((*itr), json_cls, cu.imported_module_names.size() - 1, mod);
			}
			//fix-me: check if the imported type is the same as the existing type

		}
		else //if it's a class template instance
		{
			assert(*itr == nullptr);
			int src_idx = json_cls["source"].get<int>();
			BirdeeAssert(src_idx >= 0 && src_idx < idx_to_class.size(), "Template source index out of index");
			ClassAST* src = idx_to_class[src_idx];
			BirdeeAssert(src->isTemplate(), "The source must be a class template");
			auto targs = BuildTemplateArgsFromJson(json_cls["arguments"]);
			*itr = src->template_param->GetOrCreate(targs, src, SourcePos(source_paths.size() - 1, 0, 0));
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
		//fix-me: check if the imported type is the same as the existing type
		itr++;
	}
}

string GetModulePath(const vector<string>& package)
{
	string ret = cu.homepath + "blib";
	for (auto& s : package)
	{
		ret += '/';
		ret += s;
	}
	ret += ".bmm";
	return ret;
}

string GetModulePathCurrentDir(const vector<string>& package)
{
	string ret = ".";
	for (auto& s : package)
	{
		ret += '/';
		ret += s;
	}
	ret += ".bmm";
	return ret;
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


void ImportedModule::Init(const vector<string>& package, const string& module_name)
{
	json json;
	string path = GetModulePathCurrentDir(package);
	std::ifstream in(path, std::ios::in);
	if (!in)
	{
		path = GetModulePath(package);
		in = std::ifstream(path, std::ios::in);
		if (!in)
		{
			std::cerr << "Cannot open file " << path << '\n';
			std::exit(2);
		}
	}
	in >> json;
	current_package_name = module_name;
	current_module_idx = cu.imported_module_names.size() - 1;
	idx_to_class.clear();
	orphan_idx_to_class.clear();
	source_paths.push_back(path);
	BirdeeAssert(json["Type"].get<string>() == "Birdee Module Metadata", "Bad file type");
	BirdeeAssert(json["Version"].get<double>() <= META_DATA_VERSION, "Unsupported version");
	BirdeeAssert(json["Package"] == module_name, "The module path does not fit the package name");

	auto itr = json.find("FunctionTemplates");
	if (itr != json.end())
		BuildGlobalTemplateFuncFromJson(*itr, *this);

	//we first make place holder for each class. Because classes may reference each other
	PreBuildClassFromJson(json["Classes"], module_name, *this);
	PreBuildOrphanClassFromJson(json["ImportedClasses"], *this);
	//we then create the classes
	BuildClassFromJson(json["Classes"], *this);
	BuildOrphanClassFromJson(json["ImportedClasses"], *this);

	for (auto& cls : this->classmap)
	{
		cls.second->PreGenerate();//it is safe to call multiple times
		cls.second->PreGenerateFuncs();
		//cls.second->Generate();
	}
	for (auto cls : orphan_idx_to_class)
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
}
