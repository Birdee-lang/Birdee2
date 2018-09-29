
//
#include "BdAST.h"
#include "Tokenizer.h"
#include "CompileError.h"
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <sstream>
#include "OpEnums.h"

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

 
//T can be clazz*/clazz/unique_ptr<clazz>
//"type" is the referenced type
//ToRef returns the "reference" to clazz
template<class T>
struct RefConverter
{
	using type = T;
	static T& ToRef(T& v)
	{
		return v;
	}

};

template<class T>
struct RefConverter<T*>
{
	using type = T;
	static T& ToRef(T* v)
	{
		return *v;
	}
};


template<class T>
struct RefConverter<std::unique_ptr<T>>
{
	using type = T;
	static T& ToRef(std::unique_ptr<T>& v)
	{
		return *v.get();
	}
};


//Converts some object reference/pointer/unique_ptr into reference_wrapper
template <class T>
auto GetRef(T& v)
{
	return std::reference_wrapper<T>(v);
}

template <class T>
auto GetRef(T* v)
{
	return std::reference_wrapper<T>(*v);
}

template <class T>
auto GetRef(std::unique_ptr<T>& v)
{
	return std::reference_wrapper<T>(*v);
}

//the iterator class for std::vector binding for python. 
//T can be clazz*/clazz/unique_ptr<clazz>
//will return reference_wrapper<clazz> for each access
template <class T>
struct VectorIterator
{
	size_t idx;
	std::vector <T>* vec;
	using type = typename RefConverter<T>::type;

	VectorIterator(std::vector<T>* vec, size_t idx) :vec(vec), idx(idx) {}

	VectorIterator(std::vector<T>* vec) :vec(vec) { idx = vec->size(); }

	VectorIterator& operator =(const VectorIterator& other)
	{
		vec = other.vec;
		idx = other.idx;
	}

	bool operator ==(const VectorIterator& other) const
	{
		return other.vec == vec && other.idx == idx;
	}
	bool operator !=(const VectorIterator& other) const
	{
		return !(*this==other);
	}
	VectorIterator& operator ++()
	{
		idx++;
		return *this;
	}
	static std::reference_wrapper<type> access(std::vector<T>& arr, int accidx)
	{
		return GetRef(arr[accidx]);
	}
	std::reference_wrapper<type> operator*() const
	{
		return access(*vec,idx);
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
PYBIND11_MAKE_OPAQUE(StatementASTList);
PYBIND11_MAKE_OPAQUE(std::vector<TemplateArgument>);
PYBIND11_MAKE_OPAQUE(std::vector<std::unique_ptr<ExprAST>>);
PYBIND11_MAKE_OPAQUE(std::vector<std::unique_ptr<VariableSingleDefAST>>);

//registers the vector type & iterator
//T can be clazz*/clazz/unique_ptr<clazz>
template <class T>
void RegisiterObjectVector(py::module& m,const char* name)
{
	py::class_<std::vector<T>>(m, name)
		.def(py::init<>())
		.def("pop_back", &std::vector<T>::pop_back)
		.def("__getitem__", [](std::vector<T> &v, int idx) { return VectorIterator<T>::access(v,idx); })
		//.def("__setitem__", [](const StatementASTList &v, int idx) { return v[idx].get(); })
		.def("__len__", [](const std::vector<T> &v) { return v.size(); })
		.def("__iter__", [](std::vector<T> &v) {
			return py::make_iterator(VectorIterator<T>(&v, 0), VectorIterator<T>(&v));
		}, py::keep_alive<0, 1>());


}



PYBIND11_EMBEDDED_MODULE(birdeec, m) {
	// `m` is a `py::module` which is used to bind functions and classes

	RegisiterObjectVector<std::unique_ptr<StatementAST>>(m, "StatementASTList");
	RegisiterObjectVector<std::unique_ptr<ExprAST>>(m, "ExprASTList");
	RegisiterObjectVector<TemplateArgument>(m, "TemplateArgumentList");
	RegisiterObjectVector< std::unique_ptr<VariableSingleDefAST>>(m, "VariableSingleDefASTList");

	m.def("expr", CompileExpr);
	m.def("get_cur_func", GetCurrentPreprocessedFunction);
	m.def("get_top_level", []() {return GetRef(cu.toplevel); });

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


	py::class_<PrototypeAST>(m, "PrototypeAST")
		.def_property_readonly("prefix", [](const PrototypeAST& ths) {
			return ths.prefix_idx==-1 ? cu.symbol_prefix : cu.imported_module_names[ths.prefix_idx];
		})
		.def_readwrite("name", &PrototypeAST::Name)
		.def_readwrite("return_type", &PrototypeAST::resolved_type)
		.def_property_readonly("args", [](const PrototypeAST& ths) {return GetRef(ths.resolved_args); });
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

	py::enum_<LoopControlType>(m, "LoopControlType")
		.value("BREAK", LoopControlType::BREAK)
		.value("CONTINUE", LoopControlType::CONTINUE);
	py::class_< LoopControlAST, StatementAST>(m, "LoopControlAST")
		.def_readwrite("type",(LoopControlType LoopControlAST::*)&LoopControlAST::tok);

#define SET_VALUE(A) value(#A,BinaryOp::A)
	py::enum_ < BinaryOp>(m, "BinaryOp")
		.SET_VALUE(BIN_MUL)
		.SET_VALUE(BIN_DIV)
		.SET_VALUE(BIN_MOD)
		.SET_VALUE(BIN_ADD)
		.SET_VALUE(BIN_MINUS)
		.SET_VALUE(BIN_LT)
		.SET_VALUE(BIN_GT)
		.SET_VALUE(BIN_LE)
		.SET_VALUE(BIN_GE)
		.SET_VALUE(BIN_EQ)
		.SET_VALUE(BIN_NE)
		.SET_VALUE(BIN_CMP_EQ)
		.SET_VALUE(BIN_AND)
		.SET_VALUE(BIN_XOR)
		.SET_VALUE(BIN_OR)
		.SET_VALUE(BIN_LOGIC_AND)
		.SET_VALUE(BIN_LOGIC_OR)
		.SET_VALUE(BIN_ASSIGN);
#undef SET_VALUE
	py::class_< BinaryExprAST, ExprAST>(m, "BinaryExprAST")
		.def_property_readonly("func", [](BinaryExprAST& ths) {return GetRef(ths.func); })
		.def_property_readonly("lhs", [](BinaryExprAST& ths) {return GetRef(ths.LHS); })
		.def_property_readonly("rhs", [](BinaryExprAST& ths) {return GetRef(ths.RHS); })
		.def_readwrite("op", (BinaryOp BinaryExprAST::*)&BinaryExprAST::Op);

	auto templ_arg_cls = py::class_< TemplateArgument>(m, "TemplateArgument");
	py::enum_ < TemplateArgument::TemplateArgumentType>(templ_arg_cls, "TemplateArgumentType")
		.value("TEMPLATE_ARG_TYPE", TemplateArgument::TemplateArgumentType::TEMPLATE_ARG_TYPE)
		.value("TEMPLATE_ARG_EXPR", TemplateArgument::TemplateArgumentType::TEMPLATE_ARG_EXPR);
	templ_arg_cls
		.def_readwrite("kind", &TemplateArgument::kind)
		.def_readwrite("resolved_type", &TemplateArgument::type)
		.def_property_readonly("expr", [](TemplateArgument& ths) {return GetRef(ths.expr); });
	
	py::class_< FunctionTemplateInstanceExprAST, ExprAST>(m, "FunctionTemplateInstanceExprAST")
		.def_property_readonly("template_args", [](FunctionTemplateInstanceExprAST& ths) {return GetRef(ths.template_args); })
		.def_property_readonly("expr", [](FunctionTemplateInstanceExprAST& ths) {return GetRef(ths.expr); });

	py::class_< IndexExprAST, ExprAST>(m, "IndexExprAST")
		.def_property_readonly("expr", [](IndexExprAST& ths) {return GetRef(ths.Expr); })
		.def_property_readonly("index", [](IndexExprAST& ths) {return GetRef(ths.Index); })
		.def_property_readonly("template_inst", [](IndexExprAST& ths) {return GetRef(ths.instance); })
		.def("is_template_instance",&IndexExprAST::isTemplateInstance);

	py::class_< AddressOfExprAST, ExprAST>(m, "AddressOfExprAST")
		.def_property_readonly("expr", [](AddressOfExprAST& ths) {return GetRef(ths.expr); })
		.def_readwrite("is_address_of", &AddressOfExprAST::is_address_of);

	py::class_< CallExprAST, ExprAST>(m, "CallExprAST")
		.def_property_readonly("callee", [](CallExprAST& ths) {return GetRef(ths.Callee); })
		.def_property_readonly("args", [](CallExprAST& ths) {return GetRef(ths.Args); });

	py::class_< VariableSingleDefAST, StatementAST>(m, "VariableSingleDefAST")
		.def_readwrite("name", &VariableSingleDefAST::name)
		.def_property_readonly("value", [](VariableSingleDefAST& ths) {return GetRef(ths.val); })
		.def_readwrite("resolved_type", &VariableSingleDefAST::resolved_type);

	py::class_< LocalVarExprAST, ResolvedIdentifierExprAST>(m, "LocalVarExprAST")
		.def_property_readonly("def", [](LocalVarExprAST& ths) {return GetRef(ths.def); });

}
