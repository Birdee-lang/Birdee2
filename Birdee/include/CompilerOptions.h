#pragma once
#include <string>
#include <vector>
namespace Birdee
{
	struct CompilerOptions
	{
		int optimize_level = 0;
		int size_optimize_level = 0;
		bool is_emit_ir = false;
		bool is_print_ir = false;
		bool expose_main = false;
		bool is_pic = true;
		bool is_print_symbols = false;
		std::vector<std::string> llvm_options = {"birdeec"};
	};
}
