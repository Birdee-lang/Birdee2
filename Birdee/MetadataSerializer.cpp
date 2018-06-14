#include <nlohmann/json.hpp>
#include <AST.h>
#include <ostream>
#include <assert.h>
using std::unordered_map;
using json = nlohmann::json;
using namespace Birdee;

#define META_DATA_VERSION 0.1

//fix-me: export: remember to include imported classes that is referenced by exported var/class/func
//fix-me: import: first check if the DEFINED class has already been imported in CompileModule::orphan_classes
//fix-me: import: first check if the IMPORTED class has already been imported in imported_package.find(..).class[...], then no need to add to orphan
//fix-me: import: deserialize into ImportedModule & CompileModule::orphan_classes

static unordered_map<ClassAST*, int> class_idx_map;
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
		assert(0 && "Error type");
		break;
	case tok_void:
		ret["base"] = -9;
		break;
	case tok_class:
	{
		auto itr = class_idx_map.find(type.class_ast);
		assert(itr != class_idx_map.end() && "Cannot find the class index");
		ret["base"] = itr->second;
		break;
	}
	default:
		assert(0 && "Error type");
	}
	ret["index"] = type.index_level;
	return ret;
}

const char* GetAccessModifierName(AccessModifier acc)
{
	switch (acc)
	{
	case access_private:
		return "private";
	case access_public:
		return "public";
	}
	assert(0 && "Bad access modifier");
	return nullptr;
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
	std::unique_ptr<Type> RetType;
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
	for (auto itr : Birdee::cu.classmap)
	{
		json json_cls;
		ClassAST& cls = itr.second;
		json_cls["name"] = cls.name;
		json json_fields= json::array();
		for (auto& field : cls.fields)
		{
			json json_field;
			json_field["access"] = GetAccessModifierName(field.access);
			json_field["def"] = BuildVariableJson(field.decl.get());
			json_fields.push_back(json_field);
		}
		json_cls["fields"] = std::move(json_fields);

		json json_funcs=json::array();
		for (auto& func : cls.funcs)
		{
			if (func.decl->isDeclare)
				continue;
			json json_func;
			json_func["access"] = GetAccessModifierName(func.access);
			json_func["def"] = BuildFunctionJson(func.decl.get());
			json_funcs.push_back(json_func);
		}
		json_cls["funcs"] = json_funcs;

		arr.push_back(json_cls);
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
		arr.push_back(BuildFunctionJson(&itr.second.get()));
	}
	return arr;
}

void SeralizeMetadata(std::ostream& out)
{
	json json;
	json["Type"] = "Birdee Module Metadata";
	json["Version"] = META_DATA_VERSION;
	json["Classes"] = BuildClassJson();
	json["Variables"] = BuildGlobalVaribleJson();
	json["Functions"] = BuildGlobalFuncJson();
	out << std::setw(4)<< json;
}
