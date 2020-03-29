
//
#include "BdAST.h"
#include "Tokenizer.h"
#include "CompileError.h"
#include <sstream>
#include "OpEnums.h"
#include <BindingUtil.h>


using namespace Birdee;

extern void CompileExpr(char* cmd);
extern BD_CORE_API Tokenizer tokenizer;

namespace Birdee
{
	extern BD_CORE_API bool IsResolvedTypeClass(const ResolvedType& r);
	extern BD_CORE_API string GetSourcePathByIdx(int source_idx);
}

template<typename K, typename V>
py::dict CopyMapToDict(unordered_map<K, V>& m)
{
	py::dict dict;
	py::function setfunc = py::cast<py::function>(dict.attr("__setitem__"));
	for (auto& itr : m)
	{
		setfunc(itr.first, GetRef(itr.second));
	}
	return std::move(dict);
}

template<typename K, typename V>
py::dict CopyMapToDict(unordered_map<K, std::pair<V, bool>>& m)
{
	py::dict dict;
	py::function setfunc = py::cast<py::function>(dict.attr("__setitem__"));
	for (auto& itr : m)
	{
		setfunc(itr.first, py::make_tuple(GetRef(itr.second.first), itr.second.second));
	}
	return std::move(dict);
}

void RegisiterClassForBinding2(py::module& m) {
	// `m` is a `py::module` which is used to bind functions and classes
	py::class_<UniquePtr<unique_ptr<StatementAST>>>(m, "StatementASTUniquePtr")
		.def("get", &UniquePtr<unique_ptr<StatementAST>>::get);

	RegisiterObjectVector<std::unique_ptr<StatementAST>>(m, "StatementASTList");
	RegisiterObjectVector<std::unique_ptr<ExprAST>>(m, "ExprASTList");
	RegisiterObjectVector<TemplateArgument>(m, "TemplateArgumentList");
	RegisiterObjectVector< std::unique_ptr<VariableSingleDefAST>>(m, "VariableSingleDefASTList");
	RegisiterObjectVector<TemplateParameter>(m, "TemplateParameterList");

	py::enum_<Token>(m, "BasicType")
		.value("CLASS", tok_class)
		.value("NULL", tok_null)
		.value("FUNC", tok_func)
		.value("VOID", tok_void)
		.value("BYTE", tok_byte)
		.value("SHORT", tok_short)
		.value("INT", tok_int)
		.value("LONG", tok_long)
		.value("ULONG", tok_ulong)
		.value("UINT", tok_uint)
		.value("FLOAT", tok_float)
		.value("DOUBLE", tok_double)
		.value("BOOLEAN", tok_boolean)
		.value("POINTER", tok_pointer)
		.value("PACKAGE", tok_package);

	py::class_<SourcePos>(m, "SourcePos")
		.def_readwrite("source_idx", &SourcePos::source_idx)
		.def_readwrite("line", &SourcePos::line)
		.def_readwrite("pos", &SourcePos::pos)
		.def_property_readonly("source_path", [](SourcePos& pos) {
			return GetSourcePathByIdx(pos.source_idx);
		})
		.def("__str__", &SourcePos::ToString);
	py::class_<ResolvedType>(m, "ResolvedType")
		.def(py::init<>())
		.def("__str__", &ResolvedType::GetString)
		.def_readonly("base",&ResolvedType::type)
		.def_readwrite("index_level", &ResolvedType::index_level)
		.def("is_class", &IsResolvedTypeClass)
		.def("get_detail", [](ResolvedType& ths) {
			if (ths.type == tok_class)
				return py::cast(GetRef(ths.class_ast), py::return_value_policy::reference);
			else if (ths.type == tok_package)
				return py::cast(GetRef(ths.import_node), py::return_value_policy::reference);
			else if (ths.type == tok_func)
				return py::cast(GetRef(ths.proto_ast), py::return_value_policy::reference);
			else
				return py::cast(nullptr);
		})
		.def("set_detail", [](ResolvedType& ths, Token type, py::object obj) {
			if (type == tok_class)
				ths.class_ast = py::cast<ClassAST*>(obj);
			else if (type == tok_func)
				ths.proto_ast = py::cast<PrototypeAST*>(obj);
			else if (type==tok_package)
				ths.import_node = py::cast<ImportTree*>(obj);
			else
				ths.class_ast = nullptr;
			ths.type = type;
			return;
		})
		.def("__eq__", &ResolvedType::operator ==)
		.def("is_integer", &ResolvedType::isInteger);
	py::class_<ImportedModule>(m, "ImportedModule")
		.def_readonly("source_dir", &ImportedModule::source_dir)
		.def_readonly("source_file", &ImportedModule::source_file)
		.def_readonly("is_header_only", &ImportedModule::is_header_only)
		.def("get_classmap", [](ImportedModule& ths) {
			return CopyMapToDict(ths.classmap);
		})
		.def("get_funcmap", [](ImportedModule& ths) {
			return CopyMapToDict(ths.funcmap);
		})
		.def("get_dimmap", [](ImportedModule& ths) {
			return CopyMapToDict(ths.dimmap);
		})
		.def("get_functypemap", [](ImportedModule& ths) {
			return CopyMapToDict(ths.functypemap);
		})
		.def("get_imported_classmap", [](ImportedModule& ths) {
			return CopyMapToDict(ths.imported_classmap);
		})
		.def("get_imported_funcmap", [](ImportedModule& ths) {
			return CopyMapToDict(ths.imported_funcmap);
		})
		.def("get_imported_dimmap", [](ImportedModule& ths) {
			return CopyMapToDict(ths.imported_dimmap);
		})
		.def("get_imported_functypemap", [](ImportedModule& ths) {
			return CopyMapToDict(ths.functypemap);
		});
	py::class_<ImportTree>(m, "ImportTree")
		.def("get_submodules", [](ImportTree& ths) {return CopyMapToDict(ths.map); })
		.def_property_readonly("mod", [](ImportTree& ths) {return GetRef(ths.mod); });
	py::class_<StatementAST>(m, "StatementAST")
		.def_readwrite("pos", &StatementAST::Pos)
		.def("copy", [](StatementAST& ths) {
			return new UniquePtrStatementAST(ths.Copy());
		});

	py::class_<ExprAST,StatementAST>(m, "ExprAST")
		.def_readwrite("resolved_type", &ExprAST::resolved_type)
		.def("is_lvalue", [](ExprAST& ths)->bool {return (bool)ths.GetLValue(true); });

	py::class_<AnnotationStatementAST, StatementAST>(m, "AnnotationStatementAST")
		.def_static("new", [](vector<string>&& anno, UniquePtrStatementAST& impl) {
			return new UniquePtrStatementAST(std::make_unique< AnnotationStatementAST>(std::move(anno), impl.move()));
		})
		.def_readwrite("is_expr", &AnnotationStatementAST::is_expr)
		.def_readwrite("anno",&AnnotationStatementAST::anno)
		.def_property("impl", [](AnnotationStatementAST& ths) {return GetRef(ths.impl); },
			[](AnnotationStatementAST& ths, UniquePtrStatementAST& impl) {ths.impl = impl.move(); })
		.def("run", [](AnnotationStatementAST& ths, py::object& func) {func(GetRef(ths.impl)); });

	py::class_<PrototypeAST>(m, "PrototypeAST")
		.def_property_readonly("prefix", [](const PrototypeAST& ths) {
			return ths.prefix_idx==-1 ? cu.symbol_prefix : cu.imported_module_names[ths.prefix_idx];
		})
		.def_readwrite("name", &PrototypeAST::Name)
		.def_readwrite("return_type", &PrototypeAST::resolved_type)
		.def_property_readonly("args", [](const PrototypeAST& ths) {return GetRef(ths.resolved_args); })
		.def_readwrite("is_closure", &PrototypeAST::is_closure)
		.def_readwrite("cls", &PrototypeAST::cls);

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
			return GetRef(ths.Body.body);
		})
		.def_property_readonly("proto", [](FunctionAST& ths) {
			return std::reference_wrapper<PrototypeAST>(*ths.Proto.get());
		})
		.def_readwrite("capture_on_stack", &FunctionAST::capture_on_stack)
		.def_readwrite("is_declare", &FunctionAST::isDeclare)
		.def_readwrite("is_template_instance", &FunctionAST::isTemplateInstance)
		.def_readwrite("is_imported", &FunctionAST::isImported)
		.def("run", [](FunctionAST& ths, py::object& func) {
			for (auto& v : ths.Body.body)
				func(GetRef(v));
		})
		.def_property_readonly("template_instance_args", [](FunctionAST& ths) {return GetRef(ths.template_instance_args); })
		.def_property_readonly("template_source_func", [](FunctionAST& ths) {return GetRef(ths.template_source_func); })
		.def_readwrite("capture_this",&FunctionAST::capture_this)
		//fix-me: add capture interface
		.def("is_template", &FunctionAST::isTemplate)
		.def_property_readonly("parent", [](FunctionAST& ths) {
			return std::reference_wrapper<FunctionAST>(*ths.parent);
		})
		.def_readwrite("link_name", &FunctionAST::link_name)
		.def_property_readonly("template_param", [](FunctionAST& ths) {return GetRef(*(TemplateParameterFake<FunctionAST>*)ths.template_param.get()); });

	py::class_<ResolvedIdentifierExprAST, ExprAST>(m, "ResolvedIdentifierExprAST")
		.def("is_mutable", &ResolvedIdentifierExprAST::isMutable);
	//BasicTypeExprAST
	py::class_<ArrayInitializerExprAST, ExprAST>(m, "ArrayInitializerExprAST")
		.def_static("new", [](vector<UniquePtrStatementAST*>& _expr) {
			vector<unique_ptr<ExprAST>> expr;
			expr.reserve(_expr.size());
			for (auto& e : _expr)
			{
				expr.push_back(e->move_expr());
			}
			return new UniquePtrStatementAST(
				make_unique<ArrayInitializerExprAST>(std::move(expr), tokenizer.GetSourcePos())); })
		.def_property_readonly("values", [](ArrayInitializerExprAST& ths) {return GetRef(ths.values); })
		.def("run", [](ArrayInitializerExprAST& ths, py::object& func) {
			for(auto& itr: ths.values)
				func(GetRef(itr)); });
	py::class_< ReturnAST, StatementAST>(m, "ReturnAST")
		.def_static("new", [](UniquePtrStatementAST& expr) {return new UniquePtrStatementAST(make_unique<ReturnAST>(expr.move_expr(),tokenizer.GetSourcePos())); })
		.def_property("expr", [](ReturnAST& ths) {return GetRef(ths.Val); },
			[](ReturnAST& ths, UniquePtrStatementAST& v) {ths.Val = v.move_expr(); })
		.def("run", [](ReturnAST& ths, py::object& func) {func(GetRef(ths.Val)); });
	py::class_<StringLiteralAST, ResolvedIdentifierExprAST>(m, "StringLiteralAST")
		.def_static("new", [](string& str) {return new UniquePtrStatementAST(make_unique<StringLiteralAST>(str)); })
		.def_readwrite("value",&StringLiteralAST::Val)
		.def("run", [](StringLiteralAST& ths, py::object& func) { });
	py::class_< IdentifierExprAST, ExprAST>(m, "IdentifierExprAST")
		.def_static("new", [](string& name) {return new UniquePtrStatementAST(make_unique<IdentifierExprAST>(name)); })
		.def_readwrite("name", &IdentifierExprAST::Name)
		.def_property("impl", [](IdentifierExprAST& ths) {return GetRef(ths.impl); },
			[](IdentifierExprAST& ths, UniquePtrStatementAST& v) {ths.impl = move_cast_or_throw<ResolvedIdentifierExprAST>(v.ptr); })
		.def("run", [](IdentifierExprAST& ths, py::object& func) {func(GetRef(ths.impl)); });
	py::class_< ResolvedFuncExprAST, ResolvedIdentifierExprAST>(m, "ResolvedFuncExprAST")
		.def_static("new", [](FunctionAST& f) {return new UniquePtrStatementAST(make_unique<ResolvedFuncExprAST>(&f, tokenizer.GetSourcePos())); })
		.def_readwrite("funcdef", &ResolvedFuncExprAST::def)
		.def("run", [](ResolvedFuncExprAST& ths, py::object& func) { });
	py::class_< ThisExprAST, ExprAST>(m, "ThisExprAST")
		.def_static("new", []() {return new UniquePtrStatementAST(make_unique<ThisExprAST>()); })
		.def("run", [](ThisExprAST& ths, py::object& func) {});
	py::class_< SuperExprAST, ExprAST>(m, "SuperExprAST")
		.def_static("new", [](ClassAST& cls, SourcePos pos) {return new UniquePtrStatementAST(make_unique<SuperExprAST>(&cls, pos)); })
		.def("run", [](SuperExprAST& ths, py::object& func) {});
	py::class_< BoolLiteralExprAST, ExprAST>(m, "BoolLiteralExprAST")
		.def_static("new", [](bool b) {return new UniquePtrStatementAST(make_unique<BoolLiteralExprAST>(b)); })
		.def_readwrite("value", &BoolLiteralExprAST::v)
		.def("run", [](BoolLiteralExprAST& ths, py::object& func) {});
	py::class_< IfBlockAST, StatementAST>(m,"IfBlockAST")
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
		.def_property("init_value", [](ForBlockAST& ths) {return GetRef(ths.init); },
			[](ForBlockAST& ths, UniquePtrStatementAST& v) {ths.init = v.move(); })
		.def_readwrite("loop_var", &ForBlockAST::loop_var )
		.def_property("till", [](ForBlockAST& ths) {return GetRef(ths.till); },
			[](ForBlockAST& ths, UniquePtrStatementAST& v) {ths.till = v.move_expr(); })
		.def_readwrite("inclusive", &ForBlockAST::including)
		.def_readwrite("is_dim", &ForBlockAST::isdim)
		.def_property_readonly("block", [](ForBlockAST& ths) {return GetRef(ths.block.body); })
		.def("run", [](ForBlockAST& ths, py::object& func) {
			func(GetRef(ths.init));
			func(GetRef(ths.till));
			for (auto& v : ths.block.body)
				func(GetRef(v));
		});
	py::class_< WhileBlockAST, StatementAST>(m, "WhileBlockAST")
		.def_property("cond", [](WhileBlockAST& ths) {return GetRef(ths.cond); },
			[](WhileBlockAST& ths, UniquePtrStatementAST& v) {ths.cond = v.move_expr(); })
		.def_property_readonly("block", [](WhileBlockAST& ths) {return &ths.block.body; })
		.def("run", [](WhileBlockAST& ths, py::object& func) {
			func(GetRef(ths.cond));
			for (auto& v : ths.block.body)
				func(GetRef(v));
		});

	py::enum_<LoopControlType>(m, "LoopControlType")
		.value("BREAK", LoopControlType::BREAK)
		.value("CONTINUE", LoopControlType::CONTINUE);
	py::class_< LoopControlAST, StatementAST>(m, "LoopControlAST")
		.def_static("new", [](LoopControlType ty) {return new UniquePtrStatementAST(std::make_unique<LoopControlAST>((Token)ty)); })
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
		.SET_VALUE(BIN_CMP_NE)
		.SET_VALUE(BIN_AND)
		.SET_VALUE(BIN_XOR)
		.SET_VALUE(BIN_OR)
		.SET_VALUE(BIN_LOGIC_AND)
		.SET_VALUE(BIN_LOGIC_OR)
		.SET_VALUE(BIN_ASSIGN);
#undef SET_VALUE
	py::class_< BinaryExprAST, ExprAST>(m, "BinaryExprAST")
		.def_static("new", [](BinaryOp op, UniquePtrStatementAST& lhs, UniquePtrStatementAST& rhs) {
			return new UniquePtrStatementAST(std::make_unique<BinaryExprAST>((Token)op,lhs.move_expr(),rhs.move_expr()));
		})
		.def_readwrite("is_overloaded", &BinaryExprAST::is_overloaded)
		.def_property("lhs", [](BinaryExprAST& ths) {return GetRef(ths.LHS); },
			[](BinaryExprAST& ths, UniquePtrStatementAST& v) {ths.LHS = v.move_expr(); })
		.def_property("rhs", [](BinaryExprAST& ths) {return GetRef(ths.RHS); },
			[](BinaryExprAST& ths, UniquePtrStatementAST& v) {ths.RHS = v.move_expr(); })
		.def_readwrite("op", (BinaryOp BinaryExprAST::*)&BinaryExprAST::Op)
		.def("run", [](BinaryExprAST& ths, py::object& func) {func(GetRef(ths.LHS)); func(GetRef(ths.RHS)); });

	py::enum_<UnaryOp>(m, "UnaryOp")
		.value("UNA_NOT", UNA_NOT)
		.value("UNA_ADDRESSOF", UNA_ADDRESSOF)
		.value("UNA_POINTEROF", UNA_POINTEROF)
		.value("UNA_TYPEOF", UNA_TYPEOF);
	py::class_< UnaryExprAST, ExprAST>(m, "UnaryExprAST")
		.def_static("new", [](UnaryOp op, UniquePtrStatementAST& arg) {
			return new UniquePtrStatementAST(std::make_unique<UnaryExprAST>((Token)op, arg.move_expr(), tokenizer.GetSourcePos()));
		})
		.def_readwrite("is_overloaded", &UnaryExprAST::is_overloaded)
		.def_property("arg", [](UnaryExprAST& ths) {return GetRef(ths.arg); },
			[](UnaryExprAST& ths, UniquePtrStatementAST& v) {ths.arg = v.move_expr(); })
		.def_readwrite("op", (UnaryOp UnaryExprAST::*)&UnaryExprAST::Op)
		.def("run", [](UnaryExprAST& ths, py::object& func) {func(GetRef(ths.arg)); });

	auto templ_arg_cls = py::class_< TemplateArgument>(m, "TemplateArgument");
	py::enum_ < TemplateArgument::TemplateArgumentType>(templ_arg_cls, "TemplateArgumentType")
		.value("TEMPLATE_ARG_TYPE", TemplateArgument::TemplateArgumentType::TEMPLATE_ARG_TYPE)
		.value("TEMPLATE_ARG_EXPR", TemplateArgument::TemplateArgumentType::TEMPLATE_ARG_EXPR);
	templ_arg_cls
		.def_readwrite("kind", &TemplateArgument::kind)
		.def_readwrite("resolved_type", &TemplateArgument::type)
		.def_property("expr", [](TemplateArgument& ths) {return GetRef(ths.expr); },
			[](TemplateArgument& ths, UniquePtrStatementAST& v) {ths.expr = v.move_expr(); });
	
	py::class_< FunctionTemplateInstanceExprAST, ExprAST>(m, "FunctionTemplateInstanceExprAST")
		.def_property("expr", [](FunctionTemplateInstanceExprAST& ths) {return GetRef(ths.expr); },
			[](FunctionTemplateInstanceExprAST& ths, UniquePtrStatementAST& v) {ths.expr = v.move_expr(); })
		.def_readwrite("instance", &FunctionTemplateInstanceExprAST::instance)
		.def("run", [](FunctionTemplateInstanceExprAST& ths, py::object& func) {func(GetRef(ths.expr));});

	py::class_< IndexExprAST, ExprAST>(m, "IndexExprAST")
		.def_property("expr", [](IndexExprAST& ths) {return GetRef(ths.Expr); },
			[](IndexExprAST& ths, UniquePtrStatementAST& v) {ths.Expr = v.move_expr(); })
		.def_property("index", [](IndexExprAST& ths) {return GetRef(ths.Index); },
			[](IndexExprAST& ths, UniquePtrStatementAST& v) {ths.Index = v.move_expr(); })
		.def_property("template_inst", [](IndexExprAST& ths) {return GetRef(ths.instance); },
			[](IndexExprAST& ths, UniquePtrStatementAST& v) {ths.instance = move_cast_or_throw< FunctionTemplateInstanceExprAST>(v.ptr); })
		.def("is_template_instance", [](IndexExprAST& ths) {
			unique_ptr<ExprAST>* dumy;
			return ths.isTemplateInstance(dumy);
		})
		.def("run", [](IndexExprAST& ths, py::object& func) {
			if (ths.Expr)
				func(GetRef(ths.Expr));
			if (ths.Index)
				func(GetRef(ths.Index));
			if (ths.instance)
				func(GetRef(ths.instance));
		});

	py::class_< NullExprAST, ExprAST>(m, "NullExprAST")
		.def("run", [](NullExprAST& ths, py::object& func) {
		});
	// py::class_< AddressOfExprAST, ExprAST>(m, "AddressOfExprAST")
	// 	.def_static("new", [](UniquePtrStatementAST& v, bool is_address_of) {
	// 		return new UniquePtrStatementAST(std::make_unique<AddressOfExprAST>(v.move_expr(),is_address_of,tokenizer.GetSourcePos())); 
	// 	})
	// 	.def_property("expr", [](AddressOfExprAST& ths) {return GetRef(ths.expr); },
	// 		[](AddressOfExprAST& ths, UniquePtrStatementAST& v) {ths.expr = v.move_expr(); })
	// 	.def_readwrite("is_address_of", &AddressOfExprAST::is_address_of)
	// 	.def("run", [](AddressOfExprAST& ths, py::object& func) {func(GetRef(ths.expr)); });

		py::class_< CallExprAST, ExprAST>(m, "CallExprAST")
			.def_static("new", [](UniquePtrStatementAST& callee, std::vector<UniquePtrStatementAST*> v) {
				std::vector<std::unique_ptr<ExprAST>> args;
				for (auto p : v)
				{
					args.push_back(p->move_expr());
				}
		 		return new UniquePtrStatementAST(std::make_unique<CallExprAST>(callee.move_expr(),std::move(args)));
			})
		.def_property("callee", [](CallExprAST& ths) {return GetRef(ths.Callee); },
			[](CallExprAST& ths, UniquePtrStatementAST& v) {ths.Callee = v.move_expr(); })
		.def_property_readonly("args", [](CallExprAST& ths) {return GetRef(ths.Args); })
		.def("run", [](CallExprAST& ths, py::object& func) {
			func(GetRef(ths.Callee));
			for (auto& v : ths.Args)
				func(GetRef(v));
		});

	auto cls_var_single = py::class_< VariableSingleDefAST, StatementAST>(m, "VariableSingleDefAST");
	py::enum_ < VariableSingleDefAST::CaptureType>(templ_arg_cls, "CaptureType")
		.value("CAPTURE_NONE", VariableSingleDefAST::CaptureType::CAPTURE_NONE)
		.value("CAPTURE_REF", VariableSingleDefAST::CaptureType::CAPTURE_REF)
		.value("CAPTURE_VAL", VariableSingleDefAST::CaptureType::CAPTURE_VAL);
	cls_var_single
		.def_readwrite("name", &VariableSingleDefAST::name)
		.def_property("value", [](VariableSingleDefAST& ths) {return GetRef(ths.val); },
			[](VariableSingleDefAST& ths, UniquePtrStatementAST& v) {ths.val = v.move_expr(); })
		.def_readwrite("resolved_type", &VariableSingleDefAST::resolved_type)
		.def_readwrite("capture_import_type",&VariableSingleDefAST::capture_import_type)
		.def_readwrite("capture_import_idx", &VariableSingleDefAST::capture_import_idx)
		.def_readwrite("capture_export_idx", &VariableSingleDefAST::capture_export_idx)
		.def_readwrite("capture_export_type", &VariableSingleDefAST::capture_export_type)
		.def_readwrite("is_threadlocal", &VariableSingleDefAST::is_threadlocal)
		.def("run", [](VariableSingleDefAST& ths, py::object& func) {if (ths.val)func(GetRef(ths.val)); });

	py::class_< VariableMultiDefAST, StatementAST>(m, "VariableMultiDefAST")
		.def_property_readonly("lst", [](VariableMultiDefAST& ths) {return GetRef(ths.lst); })
		.def("run", [](VariableMultiDefAST& ths, py::object& func) {
			for(auto& v:ths.lst)
				func(GetRef(v));
		});

	py::class_< LocalVarExprAST, ResolvedIdentifierExprAST>(m, "LocalVarExprAST")
		.def_static("new", [](VariableSingleDefAST* def) {
			return new UniquePtrStatementAST(std::make_unique<LocalVarExprAST>(def, tokenizer.GetSourcePos()));
		})
		.def_readwrite("vardef", &LocalVarExprAST::def)
		.def("run", [](LocalVarExprAST& ths, py::object& func) { });

	py::class_< FunctionToClosureAST, ExprAST>(m, "FunctionToClosureAST")
		.def_property("func", [](FunctionToClosureAST& ths) {return GetRef(ths.func); },
			[](FunctionToClosureAST& ths, UniquePtrStatementAST& v) {ths.func = v.move_expr(); })
		.def("run", [](FunctionToClosureAST& ths, py::object& pyfunc) { pyfunc(GetRef(ths.func)); });

	// py::class_<TypeofExprAST, ExprAST>(m, "TypeofExprAST")
	// 	.def_property("arg", [](TypeofExprAST& ths) {return GetRef(ths.arg); },
	// 		[](TypeofExprAST& ths, UniquePtrStatementAST& v) {ths.arg = v.move_expr(); })
	// 	.def("run", [](TypeofExprAST& ths, py::object& pyfunc) { pyfunc(GetRef(ths.arg)); });

	py::class_<TryBlockAST, StatementAST>(m, "TryBlockAST")
		.def_property_readonly("try_block", [](TryBlockAST& ths) {return GetRef(ths.try_block.body); })
		.def_property_readonly("catch_variables", [](TryBlockAST& ths) {return GetRef(ths.catch_variables); })
		.def("get_catch_block", [](TryBlockAST& ths,int idx) {return GetRef(ths.catch_blocks[idx].body); })
		.def("run", [](TryBlockAST& ths, py::object& pyfunc) {
			for(auto& b: ths.try_block.body)
				pyfunc(GetRef(b)); 
			for (auto& b : ths.catch_variables)
				pyfunc(GetRef(b.get()));
			for (auto& b : ths.catch_blocks)
				for(auto& itm: b.body)
					pyfunc(GetRef(itm));
		});
	py::class_<DeferBlockAST, StatementAST>(m, "DeferBlockAST")
		.def_property_readonly("defer_block", [](DeferBlockAST& ths) {return GetRef(ths.defer_block.body); })
		.def("run", [](DeferBlockAST& ths, py::object& pyfunc) {
			for (auto& b : ths.defer_block.body)
				pyfunc(GetRef(b));
		});
	py::class_<ThrowAST, StatementAST>(m, "ThrowAST")
		.def_property("expr", [](ThrowAST& ths) {return GetRef(ths.expr); },
			[](ThrowAST& ths, UniquePtrStatementAST& v) {ths.expr = v.move_expr(); })
		.def("run", [](ThrowAST& ths, py::object& pyfunc) { pyfunc(GetRef(ths.expr)); });
	auto autocomp = py::class_<AutoCompletionExprAST, ExprAST>(m, "AutoCompletionExprAST");
	py::enum_ < AutoCompletionExprAST::CompletionKind>(autocomp, "CompletionKind")
		.value("DOT", AutoCompletionExprAST::DOT)
		.value("NEW", AutoCompletionExprAST::NEW)
		.value("PARAMETER", AutoCompletionExprAST::PARAMETER);
	autocomp
		.def_property_readonly("impl", [](AutoCompletionExprAST& ths) {return GetRef(ths.impl); })
		.def_readonly("kind", &AutoCompletionExprAST::kind)
		.def_readonly("parameter_number", &AutoCompletionExprAST::parameter_number)
		.def("run", [](AutoCompletionExprAST& ths, py::object& pyfunc) { pyfunc(GetRef(ths.impl)); });
}
