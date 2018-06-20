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

static std::vector<ClassAST*> idx_to_class;
static std::vector<ClassAST*> orphan_idx_to_class;
inline void BirdeeAssert(bool b, const char* text)
{
	if (!b)
	{
		std::cerr << text<<'\n';
		abort();
	}
}

void ImportedModule::Init(vector<string>& package)
{
	idx_to_class.clear();
	orphan_idx_to_class.clear();
}

ResolvedType ConvertIdToType(json& type)
{
	ResolvedType ret;
	assert(type.type() == json::value_t::object);
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

