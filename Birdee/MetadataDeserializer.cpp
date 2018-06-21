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
	return make_unique<FunctionAST>(std::move(proto));
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

string GetModulePath(vector<string>& package)
{
	string ret = cu.homepath + "/blib";
	for (auto& s : package)
	{
		ret += '/';
		ret += s;
	}
	ret += ".bmm";
}

void ImportedModule::Init(vector<string>& package)
{
	json json;
	string path = GetModulePath(package);
	std::ifstream in(path,std::ios::in);
	if (!in)
	{
		std::cerr << "Cannot open file " << path << '\n';
		abort();
	}
	in >> json;
	current_package_name.clear();
	idx_to_class.clear();
	orphan_idx_to_class.clear();
	BirdeeAssert(json["Type"].get<string>() == "Birdee Module Metadata", "Bad file type");
	BirdeeAssert(json["Version"].get<double>() <= META_DATA_VERSION, "Unsupported version");
	//json["Classes"] = BuildClassJson();
	BuildGlobalVaribleFromJson(json["Variables"],*this);
	BuildGlobalFuncFromJson(json["Functions"],*this);
	//json["ImportedClasses"] = imported_class;
}
