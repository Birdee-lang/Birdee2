#include <BdAST.h>
#include <nlohmann/json.hpp>
#include <nlohmann/fifo_map.hpp>

#include <iomanip>
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

static unordered_map<PrototypeAST*, int> functype_idx_map;
static vector<json> exported_functype;
static long NextFunctionTypeIndex = -MAX_BASIC_TYPE_COUNT;
static unordered_map<ClassAST*, int> class_idx_map;
static vector<json> imported_class;
static long NextImportedIndex = MAX_CLASS_DEF_COUNT;
static json* defined_classes=nullptr;

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

json ConvertTypeToIndex(ResolvedType& type);

json BuildTemplateArgumentJson(TemplateArgument& arg)
{
	auto json_arg = json();
	if (arg.kind == TemplateArgument::TEMPLATE_ARG_TYPE)
	{
		json_arg["kind"] = "type";
		json_arg["type"] = ConvertTypeToIndex(arg.type);
	}
	else if (arg.kind == TemplateArgument::TEMPLATE_ARG_EXPR)
	{
		json_arg["kind"] = "expr";
		if (isa<NumberExprAST>(arg.expr.get()))
		{
			NumberExprAST* numast = static_cast<NumberExprAST *>(arg.expr.get());
			json_arg["exprtype"] = numast->resolved_type.GetString();
			std::stringstream buf;
			buf << numast->Val.v_ulong;
			json_arg["value"] = buf.str();
		}
		else if (isa<StringLiteralAST>(arg.expr.get()))
		{
			StringLiteralAST* strast= static_cast<StringLiteralAST *>(arg.expr.get());
			json_arg["exprtype"] = "string";
			json_arg["value"] = strast->Val;
		}
		else
		{
			std::cerr << "Bad TemplateArgument::expr type\n";
			abort();
		}
	}
	else
	{
		std::cerr << "Bad TemplateArgument::kind\n";
		abort();
	}
	return json_arg;
}

int ConvertClassToIndex(ClassAST* class_ast)
{
	auto itr = class_idx_map.find(class_ast);
	if (itr != class_idx_map.end())
	{
		return itr->second;
	}
	else
	{
		if (class_ast->isTemplateInstance())
		{
			if (class_ast->package_name_idx == -1) // if the template is defined in the current package
			{
				assert(class_ast->template_source_class);
				int retidx = defined_classes->size();
				class_idx_map[class_ast] = retidx;
				defined_classes->push_back(json());
				auto& the_class = defined_classes->back();
				the_class["source"] = ConvertClassToIndex(class_ast->template_source_class);
				auto args = json::array();
				for (auto& arg : *class_ast->template_instance_args)
				{
					args.push_back(BuildTemplateArgumentJson(arg));
				}
				the_class["arguments"] = args;
				//class_ast->template_instance_args
				return retidx;
			}
			//else: if the template is defined in an imported package, fall through to the next lines of codes
			//will put the class in the orphan classes
		}
		int current_idx = NextImportedIndex;
		if (NextImportedIndex == MAX_CLASS_COUNT)
		{
			std::cerr << "Too many classes imported\n";
			abort();
		}
		NextImportedIndex++;
		class_idx_map[class_ast] = current_idx;
		int out_idx = imported_class.size();
		imported_class.push_back(json());
		auto outjson = BuildSingleClassJson(*class_ast, true);
		BirdeeAssert(class_ast->package_name_idx != -1, "expecting package_name_idx!=-1");
		if (class_ast->isTemplateInstance())
		{ //if it is a template instance of an imported template, also put the template args
			auto args = json::array();
			for (auto& arg : *class_ast->template_instance_args)
			{
				args.push_back(BuildTemplateArgumentJson(arg));
			}
			outjson["template_arguments"] = args;
		}
		imported_class[out_idx] = std::move(outjson);
		return current_idx;
	}
}

json BuildVariableJson(VariableSingleDefAST* var);

json BuildPrototypeJson(PrototypeAST* func)
{
	std::unique_ptr<VariableDefAST> Args;
	json ret;
	ret["name"] = func->GetName();
	if (func->cls)
		ret["class"] = ConvertClassToIndex(func->cls);
	json args = json::array();
	for (auto& arg : func->resolved_args)
	{
		args.push_back(BuildVariableJson(arg.get()));
	}
	ret["is_closure"] = func->is_closure;
	ret["args"] = args;
	ret["return"] = ConvertTypeToIndex(func->resolved_type);
	return ret;
}

int ConvertPrototypeToIndex(PrototypeAST* proto)
{
	auto itr = functype_idx_map.find(proto);
	if (itr != functype_idx_map.end())
	{
		return itr->second;
	}
	int current_idx = NextFunctionTypeIndex;
	if (NextFunctionTypeIndex <= -MAX_CLASS_COUNT)
	{
		std::cerr << "Too many prototypes\n";
		abort();
	}
	NextFunctionTypeIndex--;
	functype_idx_map.insert(std::make_pair(proto, current_idx));
	int out_idx = exported_functype.size();
	exported_functype.push_back(json());
	exported_functype[out_idx] = BuildPrototypeJson(proto);
	return current_idx;
}
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
		ret["base"] = ConvertPrototypeToIndex(type.proto_ast);
		break;
	case tok_void:
		ret["base"] = -9;
		break;
	case tok_byte:
		ret["base"] = -10;
		break;
	case tok_class:
	{
		ret["base"] = ConvertClassToIndex(type.class_ast);
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
	if (func->isTemplateInstance)
	{
		return ret;
	}
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
void BuildAndPushFunctionJson(json& arr, FunctionAST* func)
{
	json ret = BuildFunctionJson(func);
	if (!ret.empty())
	{
		arr.push_back(std::move(ret));
	}
}

void BuildClassJson(json& arr)
{
	int idx = 0;
	for (auto itr : Birdee::cu.classmap)
	{
		class_idx_map[&itr.second.get()] = idx++;
	}
	if (class_idx_map.size() > MAX_CLASS_DEF_COUNT)
	{
		std::cerr << "Defined too many classes\n";
		abort();
	}
	for (auto itr : Birdee::cu.classmap)
	{
		arr.push_back(BuildSingleClassJson(itr.second,false));
	}
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

json BuildGlobalFuncJson(json& func_template)
{
	json arr = json::array();
	func_template = json::array();
	for (auto itr : cu.funcmap)
	{
		if (itr.second.get().isDeclare)
			continue;
		if (itr.second.get().isTemplate())
		{
			for (auto& instance : (itr.second.get().template_param->instances))
			{
				BuildAndPushFunctionJson(arr,instance.second.get());
			}
			json template_obj;
			auto ptr = itr.second.get().template_param.get();
			template_obj["template"] = ptr->source.get();
			if (ptr->annotation)
				template_obj["annotations"] = ptr->annotation->anno;
			template_obj["source_line"] = itr.second.get().Pos.line;
			func_template.push_back(std::move(template_obj));
		}
		else
			BuildAndPushFunctionJson(arr,&itr.second.get());
	}
	return arr;
}

json BuildSingleClassJson(ClassAST& cls, bool dump_qualified_name)
{
	json json_cls;
	json_cls["name"] = dump_qualified_name ? cls.GetUniqueName() : cls.name;
	json_cls["needs_rtti"] = cls.needs_rtti;
	json_cls["is_struct"] = cls.is_struct;
	if (cls.isTemplate())
	{
		assert(!cls.template_param->source.empty());
		json_cls["template"] = cls.template_param->source.get();
		json_cls["source_line"] = cls.Pos.line;
		if (cls.template_param->annotation)
			json_cls["annotations"] = cls.template_param->annotation->anno;
	}
	else
	{
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
			auto access = GetAccessModifierName(func.access);
			json_func["access"] = access;
			if (func.decl->isTemplate())
			{
				/*for (auto& instance : (func.decl->template_param->instances))
				{
					json json_instance;
					json_instance["access"] = access;
					json_instance["def"] = BuildFunctionJson(instance.second.get());
					json_funcs.push_back(json_instance);
				}*/
				//if is empty, then this function template is in a template class, 
				//find the source code in the template source
				auto& source = func.decl->template_param->source;
				assert(!source.empty());
				if (source.type==SourceStringHolder::HOLDER_STRING_VIEW)
				{
					assert(cls.isTemplateInstance());
					auto& srcmap = cls.template_source_class->template_param->source.get(); //get the template class's source
					json_func["template"] = source.get(srcmap);
				}
				else
					json_func["template"] = source.get();
				if (func.decl->template_param->annotation)
					json_func["annotations"] = func.decl->template_param->annotation->anno;
				json_func["source_line"] = func.decl->Pos.line;
			}
			else
			{
				auto funcjson = BuildFunctionJson(func.decl.get());
				if (funcjson.empty()) //if the function is a template instance, skip
					continue;
				json_func["def"] = std::move(funcjson);
			}
			json_funcs.push_back(json_func);
		}
		json_cls["funcs"] = json_funcs;
	}

	return json_cls;
}

//build placeholders for function types. We do not build functypes here, because
//classes are not resolved yet
void BuildExportedPrototypePlaceholders()
{
	for (auto& node : cu.functypemap)
	{
		functype_idx_map[node.second.get()] = NextFunctionTypeIndex--;
		exported_functype.push_back(json());
	}
	if (functype_idx_map.size() >= MAX_CLASS_DEF_COUNT)
	{
		std::cerr << "Defined too many prototypes\n";
		abort();
	}
}

//ready to build functypes after classes are built
void BuildExportedPrototypes()
{
	int idx = 0;
	for (auto& itr : cu.functypemap)
	{
		exported_functype[idx]=BuildPrototypeJson(itr.second.get());
		idx++;
	}
}

string BuildInitScript(vector<ScriptAST*>& scripts)
{
	string ret;
	for (auto& s : scripts)
	{
		ret += s->script;
		if (s->script.back() != '\n')
			ret += '\n';
	}
	return ret;
}

BD_CORE_API void SeralizeMetadata(std::ostream& out)
{
	json outjson;
	imported_class.clear();
	exported_functype.clear();
	class_idx_map.clear();
	functype_idx_map.clear();
	NextImportedIndex = MAX_CLASS_DEF_COUNT;
	NextFunctionTypeIndex = -MAX_BASIC_TYPE_COUNT;
	outjson["Type"] = "Birdee Module Metadata";
	outjson["Version"] = META_DATA_VERSION;
	outjson["Package"] = cu.symbol_prefix.substr(0, cu.symbol_prefix.size() - 1);
	outjson["Classes"] = json::array();
	defined_classes = &outjson["Classes"];

	BuildExportedPrototypePlaceholders();
	BuildClassJson(outjson["Classes"]);
	BuildExportedPrototypes();

	outjson["Variables"] = BuildGlobalVaribleJson();
	json func_template;
	outjson["Functions"] = BuildGlobalFuncJson(func_template);
	outjson["FunctionTemplates"] = func_template;
	outjson["Imports"] = cu.imports;
	outjson["ImportedClasses"] = imported_class;
	outjson["FunctionTypes"] = exported_functype;
	outjson["InitScripts"] = BuildInitScript(cu.init_scripts);
	outjson["SourceFile"] = { cu.directory , cu.filename };
	out << std::setw(4)<< outjson;
}
