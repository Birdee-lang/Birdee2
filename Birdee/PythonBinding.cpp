
//
#include "BdAST.h"
#include "Tokenizer.h"
#include "CompileError.h"
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <sstream>

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


static py::object GetNumberLiteral(NumberExprAST& ths)
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


using StatementASTList = std::vector<std::unique_ptr<StatementAST>>;
//"StatementASTList"
template <class T>
void RegisiterUniquePtrVector(py::module& m,const char* name)
{
	py::class_<std::vector<std::unique_ptr<T>>>(m, name)
		.def(py::init<>())
		.def("pop_back", &std::vector<std::unique_ptr<T>>::pop_back)
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
template <class T>
std::reference_wrapper<T> GetRef(std::unique_ptr<T>& v)
{
	return std::reference_wrapper<T>(*v.get());
}

template <class T>
std::reference_wrapper<T> GetRef(T* v)
{
	return std::reference_wrapper<T>(*v);
}

PYBIND11_MAKE_OPAQUE(StatementASTList);

PYBIND11_EMBEDDED_MODULE(birdeec, m) {
	// `m` is a `py::module` which is used to bind functions and classes
	m.def("expr", CompileExpr);
	m.def("get_cur_func", GetCurrentPreprocessedFunction);
	m.def("get_top_level", []() {return GetBasicBlock(cu.toplevel); });

	py::enum_<Token>(m, "BasicType")
		.value("class_", tok_class)
		.value("null_", tok_null)
		.value("func", tok_func)
		.value("void", tok_void)
		.value("byte", tok_byte)
		.value("int_", tok_int)
		.value("long", tok_long)
		.value("ulong", tok_ulong)
		.value("uint", tok_uint)
		.value("float_", tok_float)
		.value("double", tok_double)
		.value("boolean", tok_boolean)
		.value("pointer", tok_pointer);

	py::class_<SourcePos>(m, "SourcePos")
		.def_readwrite("source_idx", &SourcePos::source_idx)
		.def_readwrite("line", &SourcePos::line)
		.def_readwrite("pos", &SourcePos::pos)
		.def("__str__", &SourcePos::ToString);
	py::class_<ResolvedType>(m, "ResolvedType")
		.def("GetString", &ResolvedType::GetString);
		
	py::class_<StatementAST>(m, "StatementAST")
		.def_readwrite("pos", &StatementAST::Pos);
		//.def("run", [](StatementAST&) {});
	py::class_<ExprAST,StatementAST>(m, "ExprAST")
		.def_readwrite("resolved_type", &ExprAST::resolved_type)
		.def("is_lvalue", [](ExprAST& ths)->bool {return (bool)ths.GetLValue(true); });

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

	py::class_<ResolvedIdentifierExprAST, ExprAST>(m, "ResolvedIdentifierExprAST")
		.def("is_mutable", &ResolvedIdentifierExprAST::isMutable);
	py::class_<NumberExprAST, ResolvedIdentifierExprAST>(m,"NumberExprAST")
		.def("__str__", [](NumberExprAST& ths) {
			std::stringstream buf;
			ths.ToString(buf);
			return buf.str();
		})
		.def_property("value", GetNumberLiteral, [](NumberExprAST& ths,py::object& obj) {
			if (py::isinstance<py::int_>(obj))
			{
				ths.Val.v_long = obj.cast<uint64_t>();
			}
			else if (py::isinstance<py::float_>(obj))
			{
				ths.Val.v_double = obj.cast<double>();
			}
			else
			{
				abort();
			}
		})
		.def_property("type", [](NumberExprAST& ths) {return ths.Val.type; },[](NumberExprAST& ths, Token tok) {ths.Val.type = tok; });

	//BasicTypeExprAST
	py::class_< ReturnAST, StatementAST>(m, "ReturnAST")
		.def_property_readonly("expr", [](ReturnAST& ths) {return GetRef(ths.Val); });
	py::class_<StringLiteralAST, ResolvedIdentifierExprAST>(m, "StringLiteralAST")
		.def_readwrite("value",&StringLiteralAST::Val);
	py::class_< IdentifierExprAST, ExprAST>(m, "IdentifierExprAST")
		.def_readwrite("name", &IdentifierExprAST::Name)
		.def_property_readonly("impl", [](IdentifierExprAST& ths) {return GetRef(ths.impl); });
	py::class_< ResolvedFuncExprAST, ResolvedIdentifierExprAST>(m, "ResolvedFuncExprAST")
		.def_property_readonly("def", [](ResolvedFuncExprAST& ths) {return GetRef(ths.def); });
	py::class_< ThisExprAST, ExprAST>(m, "ThisExprAST");
	py::class_< BoolLiteralExprAST, ExprAST>(m, "BoolLiteralExprAST");
	py::class_< IfBlockAST, StatementAST>(m,"IFBlockAST")
		.def_property_readonly("cond", [](IfBlockAST& ths) {return GetRef(ths.cond); })
		.def_property_readonly("if_true", [](IfBlockAST& ths) {return &ths.iftrue.body; })
		.def_property_readonly("if_false", [](IfBlockAST& ths) {return &ths.iffalse.body; });
	py::class_< ForBlockAST, StatementAST>(m, "ForBlockAST")
		.def_property_readonly("init_value", [](ForBlockAST& ths) {return GetRef(ths.init); })
		.def_property_readonly("loop_var", [](ForBlockAST& ths) {return GetRef(ths.loop_var); })
		.def_property_readonly("till", [](ForBlockAST& ths) {return GetRef(ths.till); })
		.def_readwrite("inclusive", &ForBlockAST::including)
		.def_readwrite("is_dim", &ForBlockAST::isdim)
		.def_property_readonly("block", [](ForBlockAST& ths) {return &ths.block.body; });

	enum LoopControlType
	{
		BREAK = tok_break,
		CONTINUE = tok_continue
	};
	py::enum_<LoopControlType>(m, "LoopControlType")
		.value("BREAK", LoopControlType::BREAK)
		.value("CONTINUE", LoopControlType::CONTINUE);
	py::class_< LoopControlAST, StatementAST>(m, "LoopControlAST")
		.def_readwrite("type",(LoopControlType LoopControlAST::*)&LoopControlAST::tok);



}
