#include <AST.h>
#include <nlohmann/json.hpp>
#include <nlohmann/fifo_map.hpp>

#include "Metadata.h"
#include <iostream>
#include <ostream>
#include <assert.h>
#include <stdlib.h>

using std::unordered_map;
template<class K, class V, class dummy_compare, class A>
using my_workaround_fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;
using json = nlohmann::basic_json<my_workaround_fifo_map>;
using namespace Birdee;


//fix-me: export: remember to include imported classes that is referenced by exported var/class/func
//fix-me: import: first check if the DEFINED class has already been imported in CompileModule::orphan_classes
//fix-me: import: first check if the IMPORTED class has already been imported in imported_package.find(..).class[...], then no need to add to orphan
//fix-me: import: deserialize into ImportedModule & CompileModule::orphan_classes
//fix-me: generate: pre-generate extern for var/class/func

static unordered_map<ClassAST*, int> class_idx_map;
static json imported_class;
static long NextImportedIndex = MAX_CLASS_DEF_COUNT;

static const char* GetAccessModifierName(AccessModifier acc)
{
	switch (acc)
	{
	case access_private:
		return "private";
	case access_public:
		return "public";
	}
	BirdeeAssert(0 , "Bad access modifier");
	return nullptr;
}

json BuildSingleClassJson(ClassAST& cls, bool dump_qualified_name);

json ConvertTypeToIndex(ResolvedType& type)
{
	json ret;
	
	switch (type.type)
	{
	case tok_boolean:
		ret["base"] = -1;
		break;
	case tok_uint:
		ret["base"] = -2;
		break;
	case tok_int:
		ret["base"] = -3;
		break;
	case tok_long:
		ret["base"] = -4;
		break;
	case tok_ulong:
		ret["base"] = -5;
		break;
	case tok_float:
		ret["base"] = -6;
		break;
	case tok_double:
		ret["base"] = -7;
		break;
	case tok_pointer:
		ret["base"] = -8;
		break;
	case tok_func:
		BirdeeAssert(0 , "Error type");
		break;
	case tok_void:
		ret["base"] = -9;
		break;
	case tok_class:
	{
		auto itr = class_idx_map.find(type.class_ast);
		if (itr != class_idx_map.end())
		{
			ret["base"] = itr->second;
		}
		else
		{
			int current_idx = NextImportedIndex;
			if (NextImportedIndex == MAX_CLASS_COUNT)
			{
				std::cerr << "Too many classes imported\n";
				abort();
			}
			NextImportedIndex++;
			class_idx_map[type.class_ast] = current_idx;
			auto json = BuildSingleClassJson(*type.class_ast,true);
			BirdeeAssert(type.class_ast->package_name_idx!=-1 , "package_name_idx!=-1 of class should not be null");
			imported_class.push_back(std::move(json));
			ret["base"] = current_idx;		
		}
		
		break;
	}
	default:
		BirdeeAssert(0 , "Error type");
	}
	ret["index"] = type.index_level;
	return ret;
}



json BuildVariableJson(VariableSingleDefAST* var)
{
	json ret;
	ret["name"] = var->name;
	ret["type"] = ConvertTypeToIndex(var->resolved_type);
	return ret;
}

json BuildFunctionJson(FunctionAST* func)
{
	std::unique_ptr<VariableDefAST> Args;
	json ret;
	ret["name"] = func->GetName();
	json args = json::array();
	for (auto& arg : func->Proto->resolved_args)
	{
		args.push_back(BuildVariableJson(arg.get()));
	}
	ret["args"] = args;
	ret["return"] = ConvertTypeToIndex(func->Proto->resolved_type);
	return ret;
}

json BuildClassJson()
{
	json arr=json::array();
	int idx = 0;
	for (auto itr : Birdee::cu.classmap)
		class_idx_map[&itr.second.get()] = idx++;
	if (class_idx_map.size() > MAX_CLASS_DEF_COUNT)
	{
		std::cerr << "Defined too many classes\n";
		abort();
	}
	for (auto itr : Birdee::cu.classmap)
	{
		arr.push_back(BuildSingleClassJson(itr.second,false));
	}
	return arr;
}

json BuildGlobalVaribleJson()
{
	json arr = json::array();
	for (auto itr : cu.dimmap)
	{
		arr.push_back(BuildVariableJson(&itr.second.get()));
	}
	return arr;
}

json BuildGlobalFuncJson()
{
	json arr = json::array();
	for (auto itr : cu.funcmap)
	{
		if (itr.second.get().isDeclare)
			continue;
		arr.push_back(BuildFunctionJson(&itr.second.get()));
	}
	return arr;
}

json BuildSingleClassJson(ClassAST& cls, bool dump_qualified_name)
{
	json json_cls;
	json_cls["name"] = dump_qualified_name ? cls.GetUniqueName() : cls.name;
	json json_fields = json::array();
	for (auto& field : cls.fields)
	{
		json json_field;
		json_field["access"] = GetAccessModifierName(field.access);
		json_field["def"] = BuildVariableJson(field.decl.get());
		json_fields.push_back(json_field);
	}
	json_cls["fields"] = std::move(json_fields);

	json json_funcs = json::array();
	for (auto& func : cls.funcs)
	{
		json json_func;
		json_func["access"] = GetAccessModifierName(func.access);
		json_func["def"] = BuildFunctionJson(func.decl.get());
		json_funcs.push_back(json_func);
	}
	json_cls["funcs"] = json_funcs;
	return json_cls;
}

void SeralizeMetadata(std::ostream& out)
{
	json json;
	imported_class.clear();
	class_idx_map.clear();
	imported_class = json::array();
	NextImportedIndex = MAX_CLASS_DEF_COUNT;
	json["Type"] = "Birdee Module Metadata";
	json["Version"] = META_DATA_VERSION;
	json["Package"] = cu.symbol_prefix.substr(0, cu.symbol_prefix.size() - 1);
	json["Classes"] = BuildClassJson();
	json["Variables"] = BuildGlobalVaribleJson();
	json["Functions"] = BuildGlobalFuncJson();
	json["ImportedClasses"] = imported_class;
	out << std::setw(4)<< json;
}
