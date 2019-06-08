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
		tok_declare,
		tok_abstract,
		tok_import,
		tok_package,
		tok_functype,
		tok_closure,
		tok_typeof,

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
		tok_colon,
		tok_dot,
		tok_end,
		tok_class,
		tok_struct,
		tok_private,
		tok_public,
		tok_string_literal,
		tok_if,
		tok_then,
		tok_else,
		tok_return,
		tok_for,
		tok_this,
		tok_super,
		tok_null,
		tok_address_of,
		tok_pointer_of,
		tok_new,
		tok_alias,
		tok_true,
		tok_false,
		tok_to,
		tok_till,
		tok_break,
		tok_continue,
		tok_at,
		tok_script,
		tok_annotation,
		tok_while,
		tok_try,
		tok_catch,
		tok_throw,

		//type
		tok_int,
		tok_long,
		tok_ulong,
		tok_uint,
		tok_float,
		tok_double,
		tok_auto,
		tok_void,
		tok_boolean,
		tok_pointer,
		tok_byte,

		tok_dotdot,
		tok_ellipsis,
	
		tok_add,
		tok_minus,
		tok_mul,
		tok_div,
		tok_mod,
		tok_assign,
		tok_equal,
		tok_ne,
		tok_cmp_equal,
		tok_cmp_ne,
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
		tok_into,
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
		bool operator < (const NumberLiteral& v);
	};

	inline bool isTypeToken(Token t)
	{
		switch (t)
		{
		case tok_byte:
		case tok_int:
		case tok_long:
		case tok_ulong:
		case tok_uint:
		case tok_float:
		case tok_double:
		case tok_boolean:
		case tok_pointer:
			return true;
		}
		return false;
	}
	inline bool isBooleanToken(Token t)
	{
		switch (t)
		{
		case tok_equal:
		case tok_ne:
		case tok_cmp_equal:
		case tok_ge:
		case tok_le:
		case tok_logic_and:
		case tok_logic_or:
		case tok_gt:
		case tok_lt:
		case tok_and:
		case tok_or:
		case tok_not:
			return true;
		}
		return false;
	}
	inline bool isLogicToken(Token Op)
	{
		return Op == tok_logic_and || Op == tok_logic_or || Op == tok_and
			|| Op == tok_or || Op == tok_not || Op == tok_xor;
	}
}