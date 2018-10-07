#include "BdAST.h"
#include "Tokenizer.h"
#include "CompileError.h"
#include <BindingUtil.h>
#include <sstream>
#include "OpEnums.h"
#include <CastAST.h>

using namespace Birdee;


extern Birdee::Tokenizer SwitchTokenizer(Birdee::Tokenizer&& tokzr);
extern std::unique_ptr<ExprAST> ParseExpressionUnknown();
extern int ParseTopLevel();
extern FunctionAST* GetCurrentPreprocessedFunction();
extern std::unique_ptr<Type> ParseTypeName();

static unique_ptr<ExprAST> outexpr = nullptr;
struct BirdeePyContext
{
	py::scoped_interpreter guard;
	py::module main_module;
	py::object orig_scope;
	py::object copied_scope;
	BirdeePyContext()
	{
		main_module = py::module::import("__main__");
		if (cu.is_script_mode)
		{
			PyObject* newdict = PyDict_Copy(main_module.attr("__dict__").ptr());
			copied_scope = py::cast<py::object>(newdict);
		}
		py::module::import("birdeec");
		py::exec("from birdeec import *");
		orig_scope = main_module.attr("__dict__");
	}
};
static BirdeePyContext& InitPython()
{
	static BirdeePyContext context;
	return context;
}


void RunGenerativeScript()
{
	try
	{
		py::eval_file((cu.directory + "/" + cu.filename).c_str(), InitPython().copied_scope);
	}
	catch (py::error_already_set& e)
	{
		std::cerr<<e.what();
	}
	catch (std::runtime_error& e)
	{
		std::cerr << e.what();
	}
}

void Birdee::ScriptAST::Phase1()
{
	auto& env=InitPython();
	try
	{
		py::exec(script.c_str(),env.orig_scope);
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

static void CompileExpr(char* cmd) {
	Birdee::Tokenizer toknzr(std::make_unique<Birdee::StringStream>(std::string(cmd)), -1);
	toknzr.GetNextToken();
	auto old_tok = SwitchTokenizer(std::move(toknzr));
	outexpr = ParseExpressionUnknown();
	SwitchTokenizer(std::move(old_tok));
}


static ResolvedType ResolveType(string str)
{
	Birdee::Tokenizer toknzr(std::make_unique<Birdee::StringStream>(std::string(str)), -1);
	toknzr.GetNextToken();
	auto old_tok = SwitchTokenizer(std::move(toknzr));
	SourcePos pos = old_tok.GetSourcePos();
	auto type = ParseTypeName();
	SwitchTokenizer(std::move(old_tok));
	return ResolvedType(*type, pos);
}

static int CompileTopLevel(char* src)
{
	Birdee::Tokenizer toknzr(std::make_unique<Birdee::StringStream>(std::string(src)), -1);
	toknzr.GetNextToken();
	auto old_tok = SwitchTokenizer(std::move(toknzr));
	int ret = ParseTopLevel();
	SwitchTokenizer(std::move(old_tok));
	return ret;
}

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

static auto NewNumberExpr(Token tok, py::object& obj) {
	ResolvedType t;
	t.type = tok;
	if (!t.isNumber())
		throw std::invalid_argument("The type is not a number type!");

	NumberLiteral val;
	val.type = tok;
	if (py::isinstance<py::int_>(obj))
	{
		if(tok == tok_float || tok == tok_double)
			val.v_double = (double)obj.cast<uint64_t>();
		else
			val.v_long = obj.cast<uint64_t>();
	}
	else if (py::isinstance<py::float_>(obj))
	{
		if (tok!=tok_float && tok!=tok_double)
			throw std::invalid_argument("bad input type, expecting an float");
		val.v_double = obj.cast<double>();
	}
	else
	{
		throw std::invalid_argument("bad input type, must be either an integer or a float");
	}

	return new UniquePtr<unique_ptr<StatementAST>>(std::make_unique< NumberExprAST>(val));
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

namespace Birdee
{
	extern string GetTokenString(Token tok);
}

template<Token t1,Token t2>
void RegisterNumCastClass(py::module& m)
{
	string name = string("CastNumberExpr_") + GetTokenString(t1) + "_" + GetTokenString(t2);
	using T = Birdee::CastNumberExpr<t1, t2>;
	py::class_<T, ExprAST>(m, name.c_str())
		.def_property_readonly("expr", [](T& ths) {return GetRef(ths.expr); })
		.def("run", [](T& ths, py::object& func) {func(GetRef(ths.expr)); });
}

//extern template class py::enum_<Token>;
//extern template class py::class_<SourcePos>;
//extern template class py::class_<ResolvedType>;
//extern template class py::class_<StatementAST>;
//extern template class py::class_<ExprAST, StatementAST>;
//extern template class py::class_<ResolvedIdentifierExprAST, ExprAST>;

PYBIND11_MAKE_OPAQUE(std::vector<FieldDef>);
PYBIND11_MAKE_OPAQUE(std::vector<MemberFunctionDef>);

extern void RegisiterClassForBinding2(py::module& m);

PYBIND11_EMBEDDED_MODULE(birdeec, m)
{
	if (cu.is_script_mode)
	{
		m.def("clear_compile_unit", []() {cu.Clear(); });
		m.def("top_level", CompileTopLevel);
		m.def("process_top_level", []() {cu.Phase0(); cu.Phase1(); });
	}
	m.def("resolve_type", ResolveType);
	py::class_ < CompileError>(m, "CompileError")
		.def_readwrite("linenumber", &CompileError::linenumber)
		.def_readwrite("pos", &CompileError::pos)
		.def_readwrite("msg", &CompileError::msg);
	py::class_ < TokenizerError>(m, "TokenizerError")
		.def_readwrite("linenumber", &TokenizerError::linenumber)
		.def_readwrite("pos", &TokenizerError::pos)
		.def_readwrite("msg", &TokenizerError::msg);

	static py::exception<int> CompileErrorExc(m, "CompileException");
	static py::exception<int> TokenizerErrorExc(m, "TokenizerException");
	py::register_exception_translator([](std::exception_ptr p) {
		try {
			if (p) std::rethrow_exception(p);
		}
		catch (const CompileError &e) {
			CompileErrorExc(e.msg.c_str());
		}
		catch (const TokenizerError &e) {
			TokenizerErrorExc(e.msg.c_str());
		}
	});

	m.def("expr", CompileExpr);
	m.def("get_cur_func", GetCurrentPreprocessedFunction);
	m.def("get_func", [](const std::string& name) {
		auto itr = cu.funcmap.find(name);
		if (itr == cu.funcmap.end())
			return GetRef((FunctionAST*)nullptr);
		else
			return itr->second;
	});
	m.def("get_top_level", []() {return GetRef(cu.toplevel); });
	m.def("get_compile_error", []() {return GetRef(CompileError::last_error); });
	m.def("get_tokenizer_error", []() {return GetRef(TokenizerError::last_error); });

	RegisiterClassForBinding2(m);

	RegisiterObjectVector<FieldDef>(m, "FieldDefList");
	RegisiterObjectVector<MemberFunctionDef>(m, "MemberFunctionDefList");

	py::class_<NumberExprAST, ResolvedIdentifierExprAST>(m, "NumberExprAST")
		.def_static("new", NewNumberExpr)
		.def("__str__", [](NumberExprAST& ths) {
			std::stringstream buf;
			ths.ToString(buf);
			return buf.str();
		})
		.def_property("value", GetNumberLiteral, [](NumberExprAST& ths, py::object& obj) {
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
				throw std::invalid_argument("bad input type, must be either an integer or a float");
			}
		})
		.def_property("type", [](NumberExprAST& ths) {return ths.Val.type; }, [](NumberExprAST& ths, Token tok) {ths.Val.type = tok; })
		.def("run", [](NumberExprAST& ths, py::object& func) {});


	py::enum_ < AccessModifier>(m, "AccessModifier")
		.value("PUBLIC", AccessModifier::access_public)
		.value("PRIVATE", AccessModifier::access_private);

	py::class_ < FieldDef>(m, "FieldDef")
		.def_readwrite("index", &FieldDef::index)
		.def_readwrite("access", &FieldDef::access)
		.def_property_readonly("decl", [](FieldDef& ths) {return GetRef(ths.decl); });

	py::class_ < MemberFunctionDef>(m, "MemberFunctionDef")
		.def_readwrite("access", &MemberFunctionDef::access)
		.def_property_readonly("decl", [](MemberFunctionDef& ths) {return GetRef(ths.decl); });

	py::class_ < NewExprAST, ExprAST>(m, "NewExprAST")
		.def_property_readonly("args", [](NewExprAST& ths) {return GetRef(ths.args); })
		.def_property_readonly("func", [](NewExprAST& ths) {return GetRef(ths.func); })
		.def("run", [](NewExprAST& ths, py::object& func) {
			for (auto &v : ths.args)
				func(GetRef(v));
		});

	py::class_ < ClassAST, StatementAST>(m, "ClassAST")
		.def_readwrite("name", &ClassAST::name)
		.def_property_readonly("fields", [](ClassAST& ths) {return GetRef(ths.fields); })
		.def_property_readonly("funcs", [](ClassAST& ths) {return GetRef(ths.funcs); })
		.def_property_readonly("template_instance_args", [](ClassAST& ths) {return GetRef(ths.template_instance_args); })
		.def_property_readonly("template_source_class", [](ClassAST& ths) {return GetRef(ths.template_source_class); })
		.def_property_readonly("template_param", [](ClassAST& ths) {return GetRef(*(TemplateParameterFake<ClassAST>*)ths.template_param.get()); })
		.def("is_template_instance", &ClassAST::isTemplateInstance)
		.def("is_template", &ClassAST::isTemplate)
		.def("get_unique_name", &ClassAST::GetUniqueName)
		.def("run", [](LocalVarExprAST& ths, py::object& func) {});//fix-me: what to run on ClassAST?
//	unordered_map<reference_wrapper<const string>, int> fieldmap;
//	unordered_map<reference_wrapper<const string>, int> funcmap;
//	int package_name_idx = -1;


	auto member_cls = py::class_ < MemberExprAST, ResolvedIdentifierExprAST>(m, "MemberExprAST");
	py::enum_ < MemberExprAST::MemberType>(member_cls, "AccessModifier")
		.value("ERROR", MemberExprAST::MemberType::member_error)
		.value("PACKAGE", MemberExprAST::MemberType::member_package)
		.value("FIELD", MemberExprAST::MemberType::member_field)
		.value("FUNCTION", MemberExprAST::MemberType::member_function)
		.value("IMPORTED_DIM", MemberExprAST::MemberType::member_imported_dim)
		.value("IMPORTED_FUNCTION", MemberExprAST::MemberType::member_imported_function);

	member_cls
		.def_property_readonly("func", [](MemberExprAST& ths) {return ths.kind == MemberExprAST::MemberType::member_function ? GetRef(ths.func) : GetNullRef<MemberFunctionDef>(); })
		.def_property_readonly("field", [](MemberExprAST& ths) {return ths.kind == MemberExprAST::MemberType::member_field ? GetRef(ths.field) : GetNullRef<FieldDef>(); })
		.def_property_readonly("imported_func", [](MemberExprAST& ths) {return ths.kind == MemberExprAST::MemberType::member_imported_function ? GetRef(ths.import_func) : GetNullRef<FunctionAST>(); })
		.def_property_readonly("imported_dim", [](MemberExprAST& ths) {return ths.kind == MemberExprAST::MemberType::member_imported_dim ? GetRef(ths.import_dim) : GetNullRef<VariableSingleDefAST>(); })
		.def("to_string_array",&MemberExprAST::ToStringArray)
		.def_readwrite("kind",&MemberExprAST::kind)
		.def_property_readonly("obj", [](MemberExprAST& ths) {return GetRef(ths.Obj); })
		.def("run", [](MemberExprAST& ths, py::object& func) {func(GetRef(ths.Obj)); });

	py::class_ < ScriptAST, ExprAST>(m, "ScriptAST")
		.def_property_readonly("expr", [](ScriptAST& ths) {return GetRef(ths.expr); })
		.def_readwrite("script", &ScriptAST::script)
		.def("run", [](ScriptAST& ths, py::object& func) {func(GetRef(ths.expr)); });

	RegisterNumCastClass<tok_int, tok_float>(m);
	RegisterNumCastClass<tok_long, tok_float>(m);
	RegisterNumCastClass<tok_byte, tok_float>(m);
	RegisterNumCastClass<tok_int, tok_double>(m);
	RegisterNumCastClass<tok_long, tok_double>(m);
	RegisterNumCastClass<tok_byte, tok_double>(m);

	RegisterNumCastClass<tok_uint, tok_float>(m);
	RegisterNumCastClass<tok_ulong, tok_float>(m);
	RegisterNumCastClass<tok_uint, tok_double>(m);
	RegisterNumCastClass<tok_ulong, tok_double>(m);

	RegisterNumCastClass<tok_double, tok_int>(m);
	RegisterNumCastClass<tok_double, tok_long>(m);
	RegisterNumCastClass<tok_double, tok_byte>(m);
	RegisterNumCastClass<tok_float, tok_int>(m);
	RegisterNumCastClass<tok_float, tok_long>(m);
	RegisterNumCastClass<tok_float, tok_byte>(m);

	RegisterNumCastClass<tok_double, tok_uint>(m);
	RegisterNumCastClass<tok_double, tok_ulong>(m);
	RegisterNumCastClass<tok_float, tok_uint>(m);
	RegisterNumCastClass<tok_float, tok_ulong>(m);

	RegisterNumCastClass<tok_float, tok_double>(m);
	RegisterNumCastClass<tok_double, tok_float>(m);

	RegisterNumCastClass<tok_int, tok_uint>(m);
	RegisterNumCastClass<tok_long, tok_ulong>(m);

	RegisterNumCastClass<tok_uint, tok_int>(m);
	RegisterNumCastClass<tok_ulong, tok_long>(m);

	/*RegisterNumCastClass<tok_ulong, tok_ulong);
	RegisterNumCastClass<tok_long, tok_long);
	RegisterNumCastClass<tok_byte, tok_byte);
	RegisterNumCastClass<tok_uint, tok_uint);
	RegisterNumCastClass<tok_int, tok_int);
	RegisterNumCastClass<tok_float, tok_float);
	RegisterNumCastClass<tok_double, tok_double);*/

	RegisterNumCastClass<tok_int, tok_long>(m);
	RegisterNumCastClass<tok_int, tok_byte>(m);
	RegisterNumCastClass<tok_int, tok_ulong>(m);
	RegisterNumCastClass<tok_uint, tok_long>(m);
	RegisterNumCastClass<tok_uint, tok_byte>(m);
	RegisterNumCastClass<tok_uint, tok_ulong>(m);
	RegisterNumCastClass<tok_byte, tok_long>(m);
	RegisterNumCastClass<tok_byte, tok_ulong>(m);
	RegisterNumCastClass<tok_long, tok_byte>(m);
	RegisterNumCastClass<tok_ulong, tok_byte>(m);

	RegisterNumCastClass<tok_byte, tok_int>(m);
	RegisterNumCastClass<tok_byte, tok_uint>(m);
	RegisterNumCastClass<tok_long, tok_int>(m);
	RegisterNumCastClass<tok_long, tok_uint>(m);
	RegisterNumCastClass<tok_ulong, tok_int>(m);
	RegisterNumCastClass<tok_ulong, tok_uint>(m);
	
}