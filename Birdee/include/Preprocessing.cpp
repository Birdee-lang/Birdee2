#include "AST.h"
#include "CompileError.h"
#include "SourcePos.h"
#include <cassert>

using std::unordered_map;
using std::string;
using std::reference_wrapper;

using namespace Birdee;

template <typename T>
T* GetItemByName(const unordered_map<reference_wrapper<const string>, reference_wrapper<T>>& M,
	const string& name, SourcePos pos)
{
	auto itr = M.find(name);
	if (itr == M.end())
		throw CompileError(pos.line, pos.pos, "Cannot find the name: " + name);
	return &(itr->second.get());
}

namespace Birdee
{
	void ResolvedType::ResolveType(Type& type, SourcePos pos)
	{
		if (type.type == tok_identifier)
		{
			IdentifierType* ty = dynamic_cast<IdentifierType*>(&type);
			assert(ty && "Type should be a IdentifierType");
			this->class_ast = GetItemByName(cu.classmap,ty->name,pos);
		}
	}
}