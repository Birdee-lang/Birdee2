#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <LibDef.h>
namespace Birdee
{
	BD_CORE_API extern std::vector<std::string> source_paths;
	struct SourcePos
	{
		int source_idx;
		int line;
		int pos;
		BD_CORE_API std::string ToString() const;
		SourcePos(int source_idx,int line, int pos) :source_idx(source_idx),line(line), pos(pos) {}
	};
}
