#pragma once
#include <string>
#include <sstream>
#include <iomanip>

/*Meanings of "_X"
__ - _
_0 - .
_1 - !
_2 - [
_3 - ]
*/
namespace Birdee
{
	//Copied from https://stackoverflow.com/a/33447587/4790873
	//User: AndreyS Scherbakov
	template <typename I> std::string n2hexstr(I w, size_t hex_len = sizeof(I) << 1) {
		static const char* digits = "0123456789ABCDEF";
		std::string rc(hex_len, '0');
		for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
			rc[i] = digits[(w >> j) & 0x0f];
		return rc;
	}

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
			else if (name[i] == ',')
			{
				out += '_';
				out += '4';
			}
			else if (name[i] == ' ')
			{
				out += '_';
				out += '5';
			}
			else if (isalnum(name[i])) //is number of [a-z][A-Z]
			{
				out += name[i];
			}
			else
			{
				out += '_';
				out += 'x';
				out += n2hexstr((char)name[i]);
			}
		}
	}
}