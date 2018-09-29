#pragma once

#include "TokenDef.h"

namespace Birdee
{
	enum LoopControlType
	{
		BREAK = tok_break,
		CONTINUE = tok_continue
	};

	enum BinaryOp
	{
		BIN_MUL = tok_mul,
		BIN_DIV = tok_div,
		BIN_MOD = tok_mod,
		BIN_ADD = tok_add,
		BIN_MINUS = tok_minus,
		BIN_LT = tok_lt,
		BIN_GT = tok_gt,
		BIN_LE = tok_le,
		BIN_GE = tok_ge,
		BIN_EQ = tok_equal,
		BIN_NE = tok_ne,
		BIN_CMP_EQ = tok_cmp_equal,
		BIN_AND = tok_and,
		BIN_XOR = tok_xor,
		BIN_OR = tok_or,
		BIN_LOGIC_AND = tok_logic_and,
		BIN_LOGIC_OR = tok_logic_or,
		BIN_ASSIGN = tok_assign
	};
}