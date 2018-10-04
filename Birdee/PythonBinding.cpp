
//
#include "BdAST.h"
#include "Tokenizer.h"
#include "CompileError.h"
#include <sstream>
#include "OpEnums.h"
#include <BindingUtil.h>


using namespace Birdee;

extern py::object GetNumberLiteral(NumberExprAST& ths);

extern void CompileExpr(char* cmd);

void RegisiterClassForBinding2(py::module& m) {
	// `m` is a `py::module` which is used to bind functions and classes

	RegisiterObjectVector<std::unique_ptr<StatementAST>>(m, "StatementASTList");
	RegisiterObjectVector<std::unique_ptr<ExprAST>>(m, "ExprASTList");
	RegisiterObjectVector<TemplateArgument>(m, "TemplateArgumentList");
	RegisiterObjectVector< std::unique_ptr<VariableSingleDefAST>>(m, "VariableSingleDefASTList");
	RegisiterObjectVector<TemplateParameter>(m, "TemplateParameterList");

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

	py::class_<AnnotationStatementAST, StatementAST>(m, "AnnotationStatementAST")
		.def_readwrite("anno",&AnnotationStatementAST::anno)
		.def_property_readonly("impl", [](AnnotationStatementAST& ths) {return GetRef(ths.impl); })
		.def("run", [](AnnotationStatementAST& ths, py::object& func) {func(GetRef(ths.impl)); });

	py::class_<PrototypeAST>(m, "PrototypeAST")
		.def_property_readonly("prefix", [](const PrototypeAST& ths) {
			return ths.prefix_idx==-1 ? cu.symbol_prefix : cu.imported_module_names[ths.prefix_idx];
		})
		.def_readwrite("name", &PrototypeAST::Name)
		.def_readwrite("return_type", &PrototypeAST::resolved_type)
		.def_property_readonly("args", [](const PrototypeAST& ths) {return GetRef(ths.resolved_args); });
	//ClassAST * cls;

	py::class_ < TemplateParameter>(m, "TemplateParameter")
		.def_property_readonly("type", [](TemplateParameter& ths) {
			if (ths.type == nullptr)
				return tok_null;
			if (ths.type->type == tok_identifier)
				return tok_class;
			else
				return ths.type->type;
		})
	.def_readwrite("name", &TemplateParameter::name);

	RegisiterTemplateParametersClass<ClassAST>(m, "TemplateParameters_ClassAST");
	RegisiterTemplateParametersClass<FunctionAST>(m, "TemplateParameters_FunctionAST");

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
		.def("run", [](FunctionAST& ths, py::object& func) {
			for (auto& v : ths.Body.body)
				func(GetRef(v));
		})
		.def("is_template", &FunctionAST::isTemplate)
		.def_readwrite("link_name", &FunctionAST::link_name)
		.def_property_readonly("template_param", [](FunctionAST& ths) {return GetRef(*(TemplateParameterFake<FunctionAST>*)ths.template_param.get()); });

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
		.def_property("type", [](NumberExprAST& ths) {return ths.Val.type; },[](NumberExprAST& ths, Token tok) {ths.Val.type = tok; })
		.def("run", [](NumberExprAST& ths, py::object& func) {});

	//BasicTypeExprAST
	py::class_< ReturnAST, StatementAST>(m, "ReturnAST")
		.def_property_readonly("expr", [](ReturnAST& ths) {return GetRef(ths.Val); })
		.def("run", [](ReturnAST& ths, py::object& func) {func(GetRef(ths.Val)); });
	py::class_<StringLiteralAST, ResolvedIdentifierExprAST>(m, "StringLiteralAST")
		.def_readwrite("value",&StringLiteralAST::Val)
		.def("run", [](StringLiteralAST& ths, py::object& func) { });
	py::class_< IdentifierExprAST, ExprAST>(m, "IdentifierExprAST")
		.def_readwrite("name", &IdentifierExprAST::Name)
		.def_property_readonly("impl", [](IdentifierExprAST& ths) {return GetRef(ths.impl); })
		.def("run", [](IdentifierExprAST& ths, py::object& func) {func(GetRef(ths.impl)); });
	py::class_< ResolvedFuncExprAST, ResolvedIdentifierExprAST>(m, "ResolvedFuncExprAST")
		.def_property_readonly("funcdef", [](ResolvedFuncExprAST& ths) {return GetRef(ths.def); })
		.def("run", [](ResolvedFuncExprAST& ths, py::object& func) { });
	py::class_< ThisExprAST, ExprAST>(m, "ThisExprAST")
		.def("run", [](ThisExprAST& ths, py::object& func) {});
	py::class_< BoolLiteralExprAST, ExprAST>(m, "BoolLiteralExprAST")
		.def("run", [](BoolLiteralExprAST& ths, py::object& func) {});
	py::class_< IfBlockAST, StatementAST>(m,"IFBlockAST")
		.def_property_readonly("cond", [](IfBlockAST& ths) {return GetRef(ths.cond); })
		.def_property_readonly("if_true", [](IfBlockAST& ths) {return GetRef(ths.iftrue.body); })
		.def_property_readonly("if_false", [](IfBlockAST& ths) {return GetRef(ths.iffalse.body); })
		.def("run", [](IfBlockAST& ths, py::object& func) {
			func(GetRef(ths.cond));
			for (auto& v : ths.iftrue.body)
				func(GetRef(v));
			for (auto& v : ths.iffalse.body)
				func(GetRef(v));
		});
	py::class_< ForBlockAST, StatementAST>(m, "ForBlockAST")
		.def_property_readonly("init_value", [](ForBlockAST& ths) {return GetRef(ths.init); })
		.def_property_readonly("loop_var", [](ForBlockAST& ths) {return GetRef(ths.loop_var); })
		.def_property_readonly("till", [](ForBlockAST& ths) {return GetRef(ths.till); })
		.def_readwrite("inclusive", &ForBlockAST::including)
		.def_readwrite("is_dim", &ForBlockAST::isdim)
		.def_property_readonly("block", [](ForBlockAST& ths) {return &ths.block.body; })
		.def("run", [](ForBlockAST& ths, py::object& func) {
			func(GetRef(ths.init));
			func(GetRef(ths.till));
			for (auto& v : ths.block.body)
				func(GetRef(v));
		});

	py::enum_<LoopControlType>(m, "LoopControlType")
		.value("BREAK", LoopControlType::BREAK)
		.value("CONTINUE", LoopControlType::CONTINUE);
	py::class_< LoopControlAST, StatementAST>(m, "LoopControlAST")
		.def_readwrite("type",(LoopControlType LoopControlAST::*)&LoopControlAST::tok)
		.def("run", [](LoopControlAST& ths, py::object& func) {});

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
		.def_readwrite("op", (BinaryOp BinaryExprAST::*)&BinaryExprAST::Op)
		.def("run", [](BinaryExprAST& ths, py::object& func) {func(GetRef(ths.LHS)); func(GetRef(ths.RHS)); });

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
		.def_property_readonly("expr", [](FunctionTemplateInstanceExprAST& ths) {return GetRef(ths.expr); })
		.def_property_readonly("instance", [](FunctionTemplateInstanceExprAST& ths) {return GetRef(ths.instance); })
		.def("run", [](FunctionTemplateInstanceExprAST& ths, py::object& func) {func(GetRef(ths.expr));});

	py::class_< IndexExprAST, ExprAST>(m, "IndexExprAST")
		.def_property_readonly("expr", [](IndexExprAST& ths) {return GetRef(ths.Expr); })
		.def_property_readonly("index", [](IndexExprAST& ths) {return GetRef(ths.Index); })
		.def_property_readonly("template_inst", [](IndexExprAST& ths) {return GetRef(ths.instance); })
		.def("is_template_instance",&IndexExprAST::isTemplateInstance)
		.def("run", [](IndexExprAST& ths, py::object& func) {
			if (ths.Expr)
				func(GetRef(ths.Expr));
			if (ths.Index)
				func(GetRef(ths.Index));
			if (ths.instance)
				func(GetRef(ths.instance));
		});

	py::class_< AddressOfExprAST, ExprAST>(m, "AddressOfExprAST")
		.def_property_readonly("expr", [](AddressOfExprAST& ths) {return GetRef(ths.expr); })
		.def_readwrite("is_address_of", &AddressOfExprAST::is_address_of)
		.def("run", [](AddressOfExprAST& ths, py::object& func) {func(GetRef(ths.expr)); });

	py::class_< CallExprAST, ExprAST>(m, "CallExprAST")
		.def_property_readonly("callee", [](CallExprAST& ths) {return GetRef(ths.Callee); })
		.def_property_readonly("args", [](CallExprAST& ths) {return GetRef(ths.Args); })
		.def("run", [](CallExprAST& ths, py::object& func) {
			func(GetRef(ths.Callee));
			for (auto& v : ths.Args)
				func(GetRef(v));
		});

	py::class_< VariableSingleDefAST, StatementAST>(m, "VariableSingleDefAST")
		.def_readwrite("name", &VariableSingleDefAST::name)
		.def_property_readonly("value", [](VariableSingleDefAST& ths) {return GetRef(ths.val); })
		.def_readwrite("resolved_type", &VariableSingleDefAST::resolved_type)
		.def("run", [](VariableSingleDefAST& ths, py::object& func) {if (ths.val)func(GetRef(ths.val)); });

	py::class_< VariableMultiDefAST, StatementAST>(m, "VariableMultiDefAST")
		.def_property_readonly("lst", [](VariableMultiDefAST& ths) {return GetRef(ths.lst); })
		.def("run", [](VariableMultiDefAST& ths, py::object& func) {
			for(auto& v:ths.lst)
				func(GetRef(v));
		});

	py::class_< LocalVarExprAST, ResolvedIdentifierExprAST>(m, "LocalVarExprAST")
		.def_property_readonly("vardef", [](LocalVarExprAST& ths) {return GetRef(ths.def); })
		.def("run", [](LocalVarExprAST& ths, py::object& func) { });



}
