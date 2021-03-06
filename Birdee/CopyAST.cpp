#include <BdAST.h>
#include <CompileError.h>
#include <assert.h>

namespace Birdee
{
	template <typename T>
	T SetPos(T&& v, SourcePos pos)
	{
		v->Pos = pos;
		return std::move(v);
	}
	static ClassAST* cur_cls = nullptr;
	static std::unique_ptr<ExprAST> ToExpr(unique_ptr<StatementAST>&& v)
	{
		return unique_ptr_cast<ExprAST>(std::move(v));
	}
	typedef  std::unique_ptr<StatementAST>  ptrStatementAST;
	ptrStatementAST  NumberExprAST::Copy()
	{
		return SetPos(make_unique<NumberExprAST>(*this), Pos);
	}

	ptrStatementAST ReturnAST::Copy()
	{
		auto that = make_unique<ReturnAST>(Pos);
		if (Val)
		{
			that->Val = unique_ptr_cast<ExprAST>(Val->Copy());
		}
		return std::move(that);
	}

	ptrStatementAST  StringLiteralAST::Copy()
	{
		return SetPos(make_unique<StringLiteralAST>(*this),Pos);
	}

	ptrStatementAST AutoCompletionExprAST::Copy()
	{
		auto ret = make_unique<AutoCompletionExprAST>(unique_ptr_cast<ExprAST>(impl->Copy()), kind);
		ret->parameter_number = parameter_number;
		return std::move(ret);
	}

	std::unique_ptr<StatementAST> IdentifierExprAST::Copy()
	{
		auto that = make_unique<IdentifierExprAST>(Name);
		that->Pos = Pos;
		if(impl!=nullptr)
			that->impl= unique_ptr_cast<ResolvedIdentifierExprAST>(impl->Copy());
		return std::move(that);
	}

	std::unique_ptr<StatementAST> Birdee::ThisExprAST::Copy()
	{
		return SetPos(make_unique<ThisExprAST>(),Pos);
	}

	std::unique_ptr<StatementAST> Birdee::SuperExprAST::Copy()
	{
		return SetPos(make_unique<SuperExprAST>(), Pos);
	}

	std::unique_ptr<StatementAST> Birdee::BoolLiteralExprAST::Copy()
	{
		return SetPos(make_unique<BoolLiteralExprAST>(v),Pos);
	}

	std::unique_ptr<StatementAST> Birdee::NullExprAST::Copy()
	{
		return SetPos(make_unique<NullExprAST>(),Pos);
	}

	ASTBasicBlock Birdee::ASTBasicBlock::Copy()
	{
		ASTBasicBlock ret;
		for (auto& v : body)
		{
			ret.body.push_back(v->Copy());
		}
		return ret;
	}

	std::unique_ptr<StatementAST> Birdee::IfBlockAST::Copy()
	{
		return make_unique<IfBlockAST>(unique_ptr_cast<ExprAST>(cond->Copy()), iftrue.Copy(), iffalse.Copy(), Pos);
	}

	std::unique_ptr<StatementAST> Birdee::ForBlockAST::Copy()
	{
		return make_unique<ForBlockAST>(init->Copy(), unique_ptr_cast<ExprAST>(till->Copy()), including, isdim, block.Copy(), Pos);
	}
	
	std::unique_ptr<StatementAST> Birdee::LoopControlAST::Copy()
	{
		return SetPos(make_unique<LoopControlAST>(tok),Pos);
	}

	std::unique_ptr<StatementAST> Birdee::BinaryExprAST::Copy()
	{
		return make_unique<BinaryExprAST>(Op, unique_ptr_cast<ExprAST>(LHS->Copy()), unique_ptr_cast<ExprAST>(RHS->Copy()), Pos);
	}

	std::unique_ptr<StatementAST> Birdee::UnaryExprAST::Copy()
	{
		auto ret = make_unique<UnaryExprAST>(Op, unique_ptr_cast<ExprAST>(arg->Copy()), Pos);
		ret->is_overloaded = is_overloaded;
		return std::move(ret);
	}

	unique_ptr<StatementAST> Birdee::DeferBlockAST::Copy()
	{
		return  make_unique<DeferBlockAST>(defer_block.Copy(), Pos);
	}

	std::unique_ptr<StatementAST> Birdee::IndexExprAST::Copy()
	{
		return make_unique<IndexExprAST>(unique_ptr_cast<ExprAST>(Expr->Copy()), unique_ptr_cast<ExprAST>(Index->Copy()),Pos);
	}

	unique_ptr<Type> Birdee::Type::Copy()
	{
		auto v= make_unique<Type>(this->type);
		v->index_level = index_level;
		return std::move(v);
	}

	unique_ptr<Type> Birdee::PrototypeType::Copy()
	{
		assert(proto);
		auto v = make_unique<PrototypeType>(proto->Copy());
		v->index_level = index_level;
		return v;
	}

	unique_ptr<Type> Birdee::ScriptType::Copy()
	{
		string str = script;
		return make_unique<ScriptType>(str);
	}

	unique_ptr<Type> Birdee::IdentifierType::Copy()
	{
		auto v = make_unique<IdentifierType>(name);
		v->index_level = index_level;
		if (template_args)
		{
			v->template_args = make_unique<vector<unique_ptr<ExprAST>>>();
			vector<unique_ptr<ExprAST>>& vec = *template_args.get();
			for (auto& arg : vec)
			{
				v->template_args->push_back(ToExpr(arg->Copy()));
			}
		}
		return std::move(v);
	}

	unique_ptr<Type> Birdee::QualifiedIdentifierType::Copy()
	{
		vector<string> copynames = name;
		auto v = make_unique<QualifiedIdentifierType>(std::move(copynames));
		v->index_level = index_level;
		if (template_args)
		{
			v->template_args = make_unique<vector<unique_ptr<ExprAST>>>();
			vector<unique_ptr<ExprAST>>& vec = *template_args.get();
			for (auto& arg : vec)
			{
				v->template_args->push_back(ToExpr(arg->Copy()));
			}
		}
		return std::move(v);
	}

	TemplateArgument Birdee::TemplateArgument::Copy() const
	{
		return TEMPLATE_ARG_TYPE==kind?TemplateArgument(type): TemplateArgument(unique_ptr_cast<ExprAST>(expr->Copy()));
	}

	std::unique_ptr<StatementAST> Birdee::FunctionTemplateInstanceExprAST::Copy()
	{
		vector<unique_ptr<ExprAST>> args;
		for (auto& arg : raw_template_args)
		{
			args.push_back(ToExpr(arg->Copy()));
		}
		return make_unique<FunctionTemplateInstanceExprAST>(ToExpr(expr->Copy()),std::move(args),Pos);
	}

	// std::unique_ptr<StatementAST> Birdee::AddressOfExprAST::Copy()
	// {
	// 	return make_unique<AddressOfExprAST>(ToExpr(expr->Copy()),is_address_of,Pos);
	// }

	std::unique_ptr<StatementAST> Birdee::CallExprAST::Copy()
	{
		vector<unique_ptr<ExprAST>> args;
		for (auto& arg : Args)
		{
			args.push_back(ToExpr(arg->Copy()));
		}
		auto ret = make_unique<CallExprAST>(ToExpr(Callee->Copy()), std::move(args));
		ret->func_callee = func_callee;
		return SetPos(std::move(ret),Pos);
	}

	unique_ptr<VariableSingleDefAST> Birdee::VariableSingleDefAST::CopyNoInitializer()
	{
		auto ret = make_unique<VariableSingleDefAST>(name, type == nullptr ? nullptr : type->Copy(),
			nullptr, Pos);
		ret->resolved_type = resolved_type;
		ret->capture_import_type = capture_import_type;
		ret->capture_import_idx = capture_import_idx;
		ret->capture_export_type = capture_export_type;
		ret->capture_export_idx = capture_export_idx;
		ret->is_threadlocal = is_threadlocal;
		ret->is_volatile = is_volatile;
		return std::move(ret);
	}

	std::unique_ptr<StatementAST> Birdee::VariableSingleDefAST::Copy()
	{
		auto ret = make_unique<VariableSingleDefAST>(name, type == nullptr ? nullptr : type->Copy(),
			val == nullptr? nullptr:ToExpr(val->Copy()), Pos);
		ret->resolved_type = resolved_type;
		ret->capture_import_type = capture_import_type;
		ret->capture_import_idx = capture_import_idx;
		ret->capture_export_type = capture_export_type;
		ret->capture_export_idx = capture_export_idx;
		ret->is_threadlocal = is_threadlocal;
		ret->is_volatile = is_volatile;
		return std::move(ret);
	}

	std::unique_ptr<StatementAST> Birdee::VariableMultiDefAST::Copy()
	{
		vector<unique_ptr<VariableSingleDefAST>> args;
		for (auto& arg : lst)
		{
			args.push_back(unique_ptr_cast<VariableSingleDefAST>(arg->Copy()));
		}
		return make_unique<VariableMultiDefAST>(std::move(args), Pos);
	}

	std::unique_ptr<StatementAST> Birdee::LocalVarExprAST::Copy()
	{
		assert(0 && "LocalVarExprAST cannot be copied");
		return nullptr;
		//return make_unique<LocalVarExprAST>(ToExpr(def->Copy()),Pos);
	}

	std::unique_ptr<PrototypeAST> Birdee::PrototypeAST::Copy()
	{
		std::unique_ptr<PrototypeAST> ret;
		ClassAST* cls_new;
		if (!cls) //if old proto is not a class member function
			cls_new = nullptr; //new proto remain the same
		else
		{
			//else, check if we are copying a class. If so, set the cls of new proto with cur_cls
			//If not copying a class, set ret->cls=cls
			cls_new = cur_cls ? cur_cls : cls;
		}
		if (resolved_type.isResolved())
		{
			vector<unique_ptr<VariableSingleDefAST>> args;
			for (auto& arg : resolved_args)
			{
				auto value = unique_ptr_cast<VariableSingleDefAST>(arg->Copy());
				value->Phase1InFunctionType(false);
				args.emplace_back(std::move(value));
			}

			ret= make_unique<PrototypeAST>(Name, std::move(args),
				resolved_type, cls_new,  prefix_idx, is_closure);
		}
		else
		{
			ret = make_unique<PrototypeAST>(Name, Args == nullptr ? nullptr : unique_ptr_cast<VariableDefAST>(Args->Copy()),
				RetType->Copy(), cls_new, pos, is_closure);
			ret->prefix_idx = prefix_idx;
		}
		return std::move(ret);
	}

	template<typename T>
	unique_ptr<TemplateParameters<T>> Birdee::TemplateParameters<T>::Copy()
	{
		vector<TemplateParameter> newparams;
		for (auto& param : params)
		{
			newparams.push_back(std::move(TemplateParameter(param.type == nullptr ? nullptr : param.type->Copy(), param.name)));
		}
		auto ret = make_unique<TemplateParameters<T>>(std::move(newparams),is_vararg);
		ret->mod = mod;
		assert(source.type == SourceStringHolder::HOLDER_STRING_VIEW);
		ret->source.set(source.heldstr,source.view.start, source.view.len);
		return std::move(ret);
	}

	unique_ptr<StatementAST> Birdee::BasicTypeExprAST::Copy()
	{
		auto ret = make_unique<BasicTypeExprAST>();
		ret->type = type->Copy();
		return std::move(ret);
	}

	std::unique_ptr<FunctionAST> Birdee::FunctionAST::CopyNoTemplate()
	{
		if (isDeclare)
			throw CompileError(Pos, "Cannot copy a declared function");
		string vararg_n =  vararg_name;
		auto ret = make_unique<FunctionAST>(Proto->Copy(), Body.Copy(), nullptr, is_vararg,std::move(vararg_n), Pos);
		ret->isTemplateInstance = isTemplateInstance;
		ret->is_extension = is_extension;
		ret->annotation = annotation;
		return std::move(ret);
	}

	std::unique_ptr<ClassAST> Birdee::ClassAST::CopyNoTemplate()
	{
		auto clsdef = make_unique<ClassAST>(name, Pos);
		auto old_cls = cur_cls;
		cur_cls = clsdef.get();
		clsdef->package_name_idx = package_name_idx;
		std::vector<FieldDef>& nfields = clsdef->fields;
		std::vector<MemberFunctionDef>& nfuncs = clsdef->funcs;
		unordered_map<reference_wrapper<const string>, int>& nfieldmap = clsdef->fieldmap;
		unordered_map<reference_wrapper<const string>, int>& nfuncmap = clsdef->funcmap;
		int idx = 0;
		for (auto& v : fields)
		{
			FieldDef def = v.Copy();
			nfieldmap[def.decl->name] = idx;
			nfields.push_back(std::move(def));
			idx++;
		}
		idx = 0;
		for (auto& v : funcs)
		{
			auto def = v.Copy();
			nfuncmap[def.decl->GetName()] = idx;
			nfuncs.push_back(std::move(def));
			idx++;
		}
		clsdef->is_struct = is_struct;
		clsdef->is_abstract = is_abstract;
		clsdef->is_interface = is_interface;
		clsdef->needs_rtti = needs_rtti;
		if (parent_type) {
			clsdef->parent_type = parent_type->Copy();
			clsdef->parent_class = parent_class;
		}
		clsdef->self_implements = self_implements;
		clsdef->implements = implements;
		clsdef->unimpl_abstract_funcs = unimpl_abstract_funcs;
		for (auto & impl_type : self_implement_types) {
			clsdef->self_implement_types.push_back(impl_type->Copy());
		}
		clsdef->template_source_class = template_source_class;
		assert(clsdef->template_instance_args==nullptr);
		cur_cls = old_cls;
		clsdef->annotation = annotation;
		return std::move(clsdef);
	}

	std::unique_ptr<StatementAST> TryBlockAST::Copy()
	{
		vector<unique_ptr<VariableSingleDefAST>> vars;
		vector<ASTBasicBlock> bbs;
		for (auto& b : catch_blocks)
		{
			bbs.emplace_back(b.Copy());
		}
		for (auto& v : catch_variables)
		{
			vars.emplace_back(unique_ptr_cast<VariableSingleDefAST>(v->Copy()));
		}
		return make_unique<TryBlockAST>(try_block.Copy(),std::move(vars),std::move(bbs),Pos);
	}

	std::unique_ptr<StatementAST> ThrowAST::Copy()
	{
		auto ret = make_unique<ThrowAST>(ToExpr(expr->Copy()), Pos);
		return std::move(ret);
	}

	std::unique_ptr<StatementAST> Birdee::FunctionAST::Copy()
	{
		auto cpy = CopyNoTemplate();
		if (template_param)
			cpy->template_param = template_param->Copy();
		return std::move(cpy);
	}
	FieldDef Birdee::FieldDef::Copy()
	{
		return FieldDef(access,unique_ptr_cast<VariableSingleDefAST>(decl->Copy()),index);
	}
	MemberFunctionDef Birdee::MemberFunctionDef::Copy()
	{
		return MemberFunctionDef(access,unique_ptr_cast<FunctionAST>(decl->Copy()), std::vector<std::string>(*annotations), virtual_idx, is_abstract);
	}

	std::unique_ptr<StatementAST> Birdee::ResolvedFuncExprAST::Copy()
	{
		assert(0 && "Should not copy ResolvedFuncExprAST");
		return nullptr;
	}

	std::unique_ptr<StatementAST> Birdee::NewExprAST::Copy()
	{
		vector<unique_ptr<ExprAST>> cpyargs;
		for (auto& arg : args)
		{
			cpyargs.push_back(ToExpr(arg->Copy()));
		}
		return make_unique<NewExprAST>(ty->Copy(), std::move(cpyargs),method,Pos);
	}

	std::unique_ptr<StatementAST> Birdee::ClassAST::Copy()
	{
		auto clsdef = CopyNoTemplate();
		clsdef->template_param = template_param->Copy();
		return std::move(clsdef);
	}

	std::unique_ptr<StatementAST> Birdee::MemberExprAST::Copy()
	{
		return make_unique<MemberExprAST>(ToExpr(Obj->Copy()),member);
	}

	unique_ptr<StatementAST> Birdee::ScriptAST::Copy()
	{
		auto ret = make_unique<ScriptAST>(script, is_top_level);
		ret->Pos = Pos;
		if (stmt.size())
		{
			for (auto& v : stmt)
			{
				ret->stmt.emplace_back(v->Copy());
			}
		}
		return std::move(ret);
	}

	unique_ptr<StatementAST> Birdee::AnnotationStatementAST::Copy()
	{
		vector<string> anno_cp = anno;
		return make_unique<AnnotationStatementAST>(std::move(anno_cp),impl->Copy());
	}

	std::unique_ptr<StatementAST> Birdee::WhileBlockAST::Copy()
	{
		return std::make_unique<WhileBlockAST>(ToExpr(cond->Copy()),block.Copy(),Pos);
	}


	unique_ptr<StatementAST> Birdee::FunctionToClosureAST::Copy()
	{
		return make_unique<FunctionToClosureAST>(ToExpr(func->Copy()));
	}

	std::unique_ptr<StatementAST> Birdee::ArrayInitializerExprAST::Copy()
	{
		vector<unique_ptr<ExprAST>> val;
		val.reserve(values.size());
		for (auto& v : values)
			val.push_back(ToExpr(v->Copy()));
		return make_unique<ArrayInitializerExprAST>(std::move(val), Pos);
	}
}