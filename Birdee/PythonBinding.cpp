
//
#include "BdAST.h"
#include "Tokenizer.h"
#include "CompileError.h"
#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;


using namespace Birdee;
extern Birdee::Tokenizer SwitchTokenizer(Birdee::Tokenizer&& tokzr);
extern std::unique_ptr<ExprAST> ParseExpressionUnknown();
extern FunctionAST* GetCurrentPreprocessedFunction();

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

static vector<std::reference_wrapper<StatementAST>> GetBasicBlock(std::vector<std::unique_ptr<StatementAST>>& ths) {
	vector<std::reference_wrapper<StatementAST>> ret;
	for (auto& v : ths)
	{
		ret.push_back(*v.get());
	}
	return ret;
}

template <class T>
struct UniquePtrVectorIterator
{
	size_t idx;
	std::vector < std::unique_ptr<T>>* vec;

	UniquePtrVectorIterator(std::vector<std::unique_ptr<T>>* vec, size_t idx) :vec(vec), idx(idx) {}

	UniquePtrVectorIterator(std::vector<std::unique_ptr<T>>* vec) :vec(vec) { idx = vec->size(); }

	UniquePtrVectorIterator& operator =(const UniquePtrVectorIterator& other)
	{
		vec = other.vec;
		idx = other.idx;
	}

	bool operator ==(const UniquePtrVectorIterator& other) const
	{
		return other.vec == vec && other.idx == idx;
	}
	bool operator !=(const UniquePtrVectorIterator& other) const
	{
		return !(*this==other);
	}
	UniquePtrVectorIterator& operator ++()
	{
		idx++;
		return *this;
	}
	std::reference_wrapper<T> operator*() const
	{
		return *(*vec)[idx].get();
	}
};

//"StatementASTList"
template <class T>
void RegisiterUniquePtrVector(py::module& m,const char* name)
{
	py::class_<std::vector<std::unique_ptr<T>>>(m, name)
		.def(py::init<>())
		.def("pop_back", &StatementASTList::pop_back)
		/* There are multiple versions of push_back(), etc. Select the right ones. */
		//.def("push_back", (void (StatementASTList::*)(unique_ptr<StatementAST> &&)) &StatementASTList::push_back)
		//.def("back", (unique_ptr<StatementAST> &(StatementASTList::*)()) &StatementASTList::back)
		.def("__getitem__", [](const std::vector<std::unique_ptr<T>> &v, int idx) { return std::reference_wrapper<T>(* (v[idx].get())); })
		//.def("__setitem__", [](const StatementASTList &v, int idx) { return v[idx].get(); })
		.def("__len__", [](const std::vector<std::unique_ptr<T>> &v) { return v.size(); })
		.def("__iter__", [](std::vector<std::unique_ptr<T>> &v) {
		return py::make_iterator(UniquePtrVectorIterator<T>(&v, 0), UniquePtrVectorIterator<T>(&v));
	}, py::keep_alive<0, 1>());


}
using StatementASTList = std::vector<std::unique_ptr<StatementAST>>;

PYBIND11_MAKE_OPAQUE(StatementASTList);

PYBIND11_EMBEDDED_MODULE(birdeec, m) {
	// `m` is a `py::module` which is used to bind functions and classes
	m.def("expr", CompileExpr);
	m.def("get_cur_func", GetCurrentPreprocessedFunction);
	m.def("get_top_level", []() {return GetBasicBlock(cu.toplevel); });
	py::class_<SourcePos>(m, "SourcePos")
		.def_readwrite("source_idx", &SourcePos::source_idx)
		.def_readwrite("line", &SourcePos::line)
		.def_readwrite("pos", &SourcePos::pos)
		.def("__str__", &SourcePos::ToString);
	py::class_<ResolvedType>(m, "ResolvedType")
		.def("GetString", &ResolvedType::GetString);
		
	py::class_<StatementAST>(m, "StatementAST")
		.def_readwrite("pos", &StatementAST::Pos);
	py::class_<ExprAST,StatementAST>(m, "ExprAST")
		.def_readwrite("resolved_type", &ExprAST::resolved_type);

	RegisiterUniquePtrVector<StatementAST>(m, "StatementASTList");


	py::class_<PrototypeAST>(m, "PrototypeAST")
		.def_property_readonly("prefix", [](const PrototypeAST& ths) {
			return ths.prefix_idx==-1 ? cu.symbol_prefix : cu.imported_module_names[ths.prefix_idx];
		})
		.def_readwrite("name", &PrototypeAST::Name)
		.def_readwrite("return_type", &PrototypeAST::resolved_type);
	//ClassAST * cls;
	//vector<unique_ptr<VariableSingleDefAST>> resolved_args;

	py::class_<FunctionAST, ExprAST>(m, "FunctionAST")
		.def_property_readonly("body", [](FunctionAST& ths) {
			return &(ths.Body.body);
		})
		.def_property_readonly("proto", [](FunctionAST& ths) {
			return std::reference_wrapper<PrototypeAST>(*ths.Proto.get());
		})
		.def_readwrite("is_declare", &FunctionAST::isDeclare)
		.def_readwrite("is_template_instance", &FunctionAST::isTemplateInstance)
		.def_readwrite("is_imported", &FunctionAST::isImported)
		.def_readwrite("link_name", &FunctionAST::link_name);
	
	//unique_ptr<TemplateParameters<FunctionAST>> template_param;
	//std::unique_ptr<PrototypeAST> Proto;


}
