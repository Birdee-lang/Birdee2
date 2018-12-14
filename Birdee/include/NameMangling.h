#pragma once
#include <string>

/*Meanings of "_X"
__ - _
_0 - .
_1 - !
_2 - [
_3 - ]
*/
namespace Birdee
{
	inline void MangleNameAndAppend(std::string& out, const std::string& name)
	{
		out.reserve(out.size() + name.size()*1.2);
		for (int i = 0; i < name.size(); i++)
		{
			if (name[i] == '_')
			{
				out += '_';
				out += '_';
			}
			else if (name[i] == '.')
			{
				out += '_';
				out += '0';
			}
			else if (name[i] == '[')
			{
				out += '_';
				out += '2';
			}
			else if (name[i] == ']')
			{
				out += '_';
				out += '3';
			}
			else
			{
				out += name[i];
			}
		}
	}
}