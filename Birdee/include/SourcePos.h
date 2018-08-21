#pragma once
#include <string>
#include <sstream>
#include <vector>
namespace Birdee
{
	extern std::vector<std::string> source_paths;
	struct SourcePos
	{
		int source_idx;
		int line;
		int pos;
		std::string ToString()
		{
			std::stringstream buf;
			buf << "File: "<< source_paths[source_idx] <<" Line: " << line << " Pos: " << pos;
			return buf.str();
		}
		SourcePos(int source_idx,int line, int pos) :source_idx(source_idx),line(line), pos(pos) {}
	};
}
