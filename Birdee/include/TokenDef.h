#pragma once
#include <stdint.h>
namespace Birdee
{
	enum Token {
		tok_error = 0,
		tok_eof = 1,

		// commands
		tok_def,
		tok_extern,
		tok_func,

		// primary
		tok_identifier,
		tok_number,
		tok_left_bracket,
		tok_right_bracket,
		tok_left_index,
		tok_right_index,
		tok_newline,
		tok_dim,
		tok_as,
		tok_comma,
		tok_dot,
		tok_end,
		tok_class,
		tok_private,
		tok_public,
		tok_string_literal,
		tok_if,
		tok_then,
		tok_else,
		tok_return,
		tok_for,

		//type
		tok_int,
		tok_long,
		tok_ulong,
		tok_uint,
		tok_float,
		tok_double,
		tok_auto,
		tok_void,

		tok_add,
		tok_minus,
		tok_mul,
		tok_div,
		tok_mod,
		tok_assign,
		tok_equal,
		tok_ne,
		tok_cmp_equal,
		tok_ge,
		tok_le,
		tok_logic_and,
		tok_logic_or,
		tok_gt,
		tok_lt,
		tok_and,
		tok_or,
		tok_not,
		tok_xor,
	};

	enum NodeType {
		type_int,
		type_long,
		type_uint,
		type_ulong,
		type_float,
		type_double,
	};
	struct NumberLiteral {
		union {
			double v_double;
			int32_t v_int;
			int64_t v_long;
			uint32_t v_uint;
			uint64_t v_ulong;
		};
		Token type;
	};
}