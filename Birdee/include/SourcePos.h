#pragma once
#include <string>
#include <sstream>
namespace Birdee
{
	struct SourcePos
	{
		int line;
		int pos;
		std::string ToString()
		{
			std::stringstream buf;
			buf << "Line: " << line << " Pos: " << pos;
			return buf.str();
		}
		SourcePos(int line, int pos) :line(line), pos(pos) {}
	};
}
