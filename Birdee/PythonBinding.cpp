
//
#include "BdAST.h"
#include "Tokenizer.h"
#include "CompileError.h"
#include <pybind11/embed.h>

namespace py = pybind11;


using namespace Birdee;
extern Birdee::Tokenizer SwitchTokenizer(Birdee::Tokenizer&& tokzr);
extern std::unique_ptr<ExprAST> ParseExpressionUnknown();

struct BirdeePyContext
{
	py::scoped_interpreter guard;
	BirdeePyContext()
	{
		py::module::import("birdeec");
		py::exec("from birdeec import *");
	}
};
static void InitPython()
{
	static BirdeePyContext context;
}

static unique_ptr<ExprAST> outexpr=nullptr;
static void CompileExpr(char* cmd) {
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



PYBIND11_EMBEDDED_MODULE(birdeec, m) {
	// `m` is a `py::module` which is used to bind functions and classes
	m.def("expr", CompileExpr);
}
