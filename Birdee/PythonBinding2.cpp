#include "BdAST.h"
#include "Tokenizer.h"
#include "CompileError.h"
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <sstream>
#include "OpEnums.h"

using namespace Birdee;
extern Birdee::Tokenizer SwitchTokenizer(Birdee::Tokenizer&& tokzr);
extern std::unique_ptr<ExprAST> ParseExpressionUnknown();


struct BirdeePyContext
{
	py::scoped_interpreter guard;
	py::module main_module;
	BirdeePyContext()
	{
		main_module = py::module::import("__main__");
		py::module::import("birdeec");
		py::exec("from birdeec import *");
	}
};
static BirdeePyContext& InitPython()
{
	static BirdeePyContext context;
	return context;
}

static unique_ptr<ExprAST> outexpr = nullptr;
void CompileExpr(char* cmd) {
	Birdee::Tokenizer toknzr(std::make_unique<Birdee::StringStream>(std::string(cmd)), -1);
	toknzr.GetNextToken();
	auto old_tok = SwitchTokenizer(std::move(toknzr));
	outexpr = ParseExpressionUnknown();
	SwitchTokenizer(std::move(old_tok));
}



void Birdee::ScriptAST::Phase1()
{
	InitPython();
	try
	{
		py::exec(script.c_str());
	}
	catch (py::error_already_set& e)
	{
		throw CompileError(Pos.line, Pos.pos, string("\nScript exception:\n") + e.what());
	}
	expr = std::move(outexpr);
	outexpr = nullptr;
	if (expr)
	{
		expr->Phase1();
		resolved_type = expr->resolved_type;
	}
}


py::object GetNumberLiteral(NumberExprAST& ths)
{
	switch (ths.Val.type)
	{
	case tok_byte:
		return py::int_(ths.Val.v_int);
		break;
	case tok_int:
		return py::int_(ths.Val.v_int);
		break;
	case tok_long:
		return py::int_(ths.Val.v_long);
		break;
	case tok_uint:
		return py::int_(ths.Val.v_uint);
		break;
	case tok_ulong:
		return py::int_(ths.Val.v_ulong);
		break;
	case tok_float:
		return py::float_(ths.Val.v_double);
		break;
	case tok_double:
		return py::float_(ths.Val.v_double);
		break;
	}
	abort();
	return py::int_(0);
}


void Birdee::AnnotationStatementAST::Phase1()
{
	impl->Phase1();
	auto& main_module = InitPython().main_module;
	try
	{
		for (auto& func_name : anno)
		{
			main_module.attr(func_name.c_str())(GetRef(impl));
		}
	}
	catch (py::error_already_set& e)
	{
		throw CompileError(Pos.line, Pos.pos, string("\nScript exception:\n") + e.what());
	}
}


