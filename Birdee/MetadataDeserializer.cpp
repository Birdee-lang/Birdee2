#include <AST.h>
#include <nlohmann/json.hpp>
#include <nlohmann/fifo_map.hpp>

#include "Metadata.h"
#include <iostream>
#include <ostream>
#include <fstream>
#include <assert.h>
#include <stdlib.h>

using std::unordered_map;
template<class K, class V, class dummy_compare, class A>
using my_workaround_fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;
using json = nlohmann::basic_json<my_workaround_fifo_map>;
using namespace Birdee;

static std::vector<ClassAST*> idx_to_class;
static std::vector<ClassAST*> orphan_idx_to_class;
static string current_package_name;
static int current_module_idx;

extern std::vector<std::string> Birdee::source_paths;

/*
fix-me: Load template class & functions & instances
link templates with instances by name (remember find them in orphans)
Template class' template functions are not included in the metadata of class template instances, which are already included in the class template.
remember to restore the functions in the cls template instances!
*/

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
	else if(acc == "public")
		return access_public;
	BirdeeAssert(0 , "Bad access modifier");
	return access_public;
}

unique_ptr<VariableSingleDefAST> BuildVariableFromJson(const json& var)
{
	unique_ptr<VariableSingleDefAST> ret=make_unique<VariableSingleDefAST>(var["name"].get<string>(),
		ConvertIdToType(var["type"]));
	return std::move(ret);
}

void BuildGlobalVaribleFromJson(const json& globals,ImportedModule& mod)
{
	BirdeeAssert(globals.is_array(), "Expected a JSON array");
	for (auto& itr : globals)
	{
		auto var=BuildVariableFromJson(itr);
		var->PreGenerateExternForGlobal(current_package_name);
		mod.dimmap[var->name] = std::move(var);
	}
}


unique_ptr<FunctionAST> BuildFunctionFromJson(const json& func,ClassAST* cls)
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
	auto proto = make_unique<PrototypeAST>(name,std::move(Args),ConvertIdToType(func["return"]),cls, current_module_idx);
	auto protoptr = proto.get();
	auto ret= make_unique<FunctionAST>(std::move(proto));
	ret->resolved_type.type = tok_func;
	ret->resolved_type.proto_ast = protoptr;
	return std::move(ret);
}

void BuildGlobalFuncFromJson(const json& globals, ImportedModule& mod)
{
	BirdeeAssert(globals.is_array(), "Expected a JSON array");
	for (auto& itr : globals)
	{
		auto var = BuildFunctionFromJson(itr,nullptr);
		var->PreGenerate();
		mod.funcmap[var->Proto->GetName()] = std::move(var);
	}
}

void BuildSingleClassFromJson(ClassAST* ret,const json& json_cls, int module_idx)
{
	ret->name = json_cls["name"].get<string>();
	ret->package_name_idx = module_idx;
	const json& json_fields = json_cls["fields"];
	BirdeeAssert(json_fields.is_array(), "Expected an JSON array");
	int idx = 0;
	for (auto& field : json_fields)
	{
		AccessModifier acc= GetAccessModifier(field["access"].get<string>());
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
		auto funcdef = BuildFunctionFromJson(func["def"],ret);
		const string& str = funcdef->Proto->GetName();
		ret->funcs.push_back(MemberFunctionDef(acc, std::move(funcdef)));
		ret->funcmap[str] = idx;
		idx++;
	}
}

void PreBuildClassFromJson(const json& cls, const string& module_name,ImportedModule& mod)
{
	BirdeeAssert(cls.is_array(), "Expeccted a JSON array");
	for (auto& json_cls : cls)
	{
		string name=json_cls["name"].get<string>();
		auto orphan = cu.orphan_class.find(module_name + '.' + name);
		unique_ptr<ClassAST> classdef;
		if (orphan != cu.orphan_class.end())
		{
			classdef = std::move(orphan->second);
			cu.orphan_class.erase(orphan);
		}
		else
		{
			classdef = make_unique<ClassAST>(string(), SourcePos(source_paths.size()-1, 0, 0)); //add placeholder
		}
		idx_to_class.push_back(classdef.get());
		mod.classmap[name]=std::move(classdef);
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
			auto idx=name.find_last_of('.');
			BirdeeAssert(idx != string::npos && idx!=name.size()-1, "Invalid imported class name");
			auto node=cu.imported_packages.Contains(name, idx);
			if (node)
			{//if the package is imported
				auto itr = node->mod->classmap.find(name.substr(idx + 1));
				BirdeeAssert(itr != node->mod->classmap.end(), "Module imported, but cannot find the class");
				classdef=itr->second.get();
			}
			else
			{
				auto newclass = make_unique<ClassAST>(string(), SourcePos(source_paths.size()-1,0, 0)); //add placeholder
				classdef = newclass.get();
				cu.orphan_class[name]=std::move(newclass);
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
		if ((*itr)->name.size() == 0) // if it is a placeholder
		{
			BuildSingleClassFromJson((*itr), json_cls, cu.imported_module_names.size() - 1);
		}
		//fix-me: check if the imported type is the same as the existing type
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
			BuildSingleClassFromJson((*itr), json_cls, -2); //orphan classes, module index=-2
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
void ImportedModule::Init(const vector<string>& package,const string& module_name)
{
	json json;
	string path = GetModulePathCurrentDir(package);
	std::ifstream in(path,std::ios::in);
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
	current_package_name=module_name;
	current_module_idx = cu.imported_module_names.size() - 1;
	idx_to_class.clear();
	orphan_idx_to_class.clear();
	source_paths.push_back(path);
	BirdeeAssert(json["Type"].get<string>() == "Birdee Module Metadata", "Bad file type");
	BirdeeAssert(json["Version"].get<double>() <= META_DATA_VERSION, "Unsupported version");
	BirdeeAssert(json["Package"] == module_name, "The module path does not fit the package name");
	//we first make place holder for each class. Because classes may reference each other
	PreBuildClassFromJson(json["Classes"],module_name,*this);
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


	BuildGlobalVaribleFromJson(json["Variables"],*this);
	BuildGlobalFuncFromJson(json["Functions"],*this);
}
