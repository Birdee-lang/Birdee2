#include "BdAST.h"
#include "CompileError.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"

extern llvm::IRBuilder<> builder;

#define CompileAssert(a, p ,msg)\
do\
{\
	if (!(a))\
	{\
		throw CompileError(p, msg);\
	}\
}while(0)\

extern Birdee::ClassAST* GetTypeInfoType();
extern llvm::Constant* GetOrCreateTypeInfoGlobal(Birdee::ClassAST* cls);
extern llvm::Constant* GetOrCreateTypeInfoGlobal(
	Birdee::ClassAST* cls, Birdee::ClassAST* interface, int table_size, int vtable_idx);
namespace Birdee
{
	extern llvm::Type* GetLLVMTypeFromResolvedType(const ResolvedType& ty);
	extern int GetLLVMTypeSizeInBit(llvm::Type* ty);
	extern bool IsResolvedTypeClass(const ResolvedType& r);
	BD_CORE_API extern std::pair<int, FieldDef*> FindClassField(ClassAST* class_ast, const string& member);
	extern ClassAST* GetStringClass();

	static void CheckIntrinsic(FunctionAST* func, int arg_num, int targ_num)
	{
		CompileAssert(func->isTemplateInstance, func->Pos, "The intrinsic function should be a template instance");
		CompileAssert(func->Proto->resolved_args.size() == arg_num, func->Pos, "Wrong number of parameters for the intrinsic function");
		CompileAssert(func->template_instance_args->size() == targ_num, func->Pos, "Wrong number of template parameters for the intrinsic function");
	}

	static void Intrinsic_UnsafePtrCast_Phase1(FunctionAST* func)
	{
		CheckIntrinsic(func, 1, 2);
		auto& targs = *func->template_instance_args;
		CompileAssert(targs[0].kind == TemplateArgument::TEMPLATE_ARG_TYPE &&
			(targs[0].type.isReference() || targs[0].type.type == tok_pointer || targs[0].type.isInteger()), //no need to check index level. it is implied in isReference()
			func->Pos, "The 1st template argument for unsafe.ptr_cast should be a pointer, reference or integer");
		CompileAssert(targs[1].kind == TemplateArgument::TEMPLATE_ARG_TYPE &&
			(targs[1].type.isReference() || targs[1].type.type == tok_pointer || targs[1].type.isInteger()), //no need to check index level. it is implied in isReference()
			func->Pos, "The 2nd template argument for unsafe.ptr_cast should be a pointer, reference or integer");
	}

	static void Intrinsic_UnsafePtrLoad_Phase1(FunctionAST* func)
	{
		CheckIntrinsic(func, 1, 1);
		auto& targs = *func->template_instance_args;
		CompileAssert(targs[0].kind == TemplateArgument::TEMPLATE_ARG_TYPE,
			func->Pos, "The 1st template argument for unsafe.ptr_load should be a type");
		auto& arg_t = func->Proto->resolved_args[0]->resolved_type;
		CompileAssert(arg_t.type == tok_pointer || arg_t.index_level == 0,
			func->Pos, "The parameter for unsafe.ptr_load should be a pointer");
	}


	static llvm::Value* Intrinsic_UnsafePtrCast_Generate(FunctionAST* func, llvm::Value* obj, vector<unique_ptr<ExprAST>>& args)
	{
		assert(func->template_instance_args);
		assert(func->template_instance_args->size() == 2);
		assert(args.size() == 1);
		auto& targs = *func->template_instance_args;
		auto v = args[0]->Generate();
		auto ty = GetLLVMTypeFromResolvedType(targs[0].type);
		if (targs[1].type.isInteger())
		{
			//targs[0] is target type
			//targs[1] is source type
			if (targs[0].type.isInteger())
			{
				//if both src and dest are integer, generate int cast
				//extension rule is based on whether src is signed
				return builder.CreateIntCast(v, ty, targs[1].type.isSigned());
			}
			return builder.CreateIntToPtr(v, ty);
		}
		return builder.CreatePointerCast(v, ty);
	}

	static llvm::Value* Intrinsic_UnsafePtrLoad_Generate(FunctionAST* func, llvm::Value* obj, vector<unique_ptr<ExprAST>>& args)
	{
		assert(func->template_instance_args);
		assert(func->template_instance_args->size() == 1);
		assert(args.size() == 1);
		auto& targs = *func->template_instance_args;
		auto v = args[0]->Generate();
		auto ty = GetLLVMTypeFromResolvedType(targs[0].type);
		auto ptr = builder.CreatePointerCast(v, ty->getPointerTo());
		return builder.CreateLoad(ptr);
	}

	static void Intrinsic_UnsafePtrStore_Phase1(FunctionAST* func)
	{
		CheckIntrinsic(func, 2, 1);
		auto& targs = *func->template_instance_args;
		CompileAssert(targs[0].kind == TemplateArgument::TEMPLATE_ARG_TYPE,
			func->Pos, "The 1st template argument for unsafe.ptr_store should be a type");
		auto& arg_t = func->Proto->resolved_args[0]->resolved_type;
		CompileAssert(arg_t.type == tok_pointer || arg_t.index_level == 0,
			func->Pos, "The parameter for unsafe.ptr_store should be a pointer");
	}

	static llvm::Value* Intrinsic_UnsafePtrStore_Generate(FunctionAST* func, llvm::Value* obj, vector<unique_ptr<ExprAST>>& args)
	{
		assert(func->template_instance_args);
		assert(func->template_instance_args->size() == 1);
		assert(args.size() == 2);
		auto& targs = *func->template_instance_args;
		auto v = args[0]->Generate();
		auto ty = GetLLVMTypeFromResolvedType(targs[0].type);
		auto ptr = builder.CreatePointerCast(v, ty->getPointerTo());
		auto v2 = args[1]->Generate();
		return builder.CreateStore(v2, ptr);
	}

	static void Intrinsic_UnsafeBitCast_Phase1(FunctionAST* func)
	{
		CheckIntrinsic(func, 1, 2);
		auto& targs = *func->template_instance_args;
		CompileAssert(targs[0].kind == TemplateArgument::TEMPLATE_ARG_TYPE && targs[1].kind == TemplateArgument::TEMPLATE_ARG_TYPE,
			func->Pos, "The 1st and 2nd template arguments for unsafe.bit_cast should be a type");
	}

	static llvm::Value* Intrinsic_UnsafeBitCast_Generate(FunctionAST* func, llvm::Value* obj, vector<unique_ptr<ExprAST>>& args)
	{
		assert(func->template_instance_args);
		assert(func->template_instance_args->size() == 2);
		assert(args.size() == 1);
		auto& targs = *func->template_instance_args;
		auto v = args[0]->Generate();
		auto ty = GetLLVMTypeFromResolvedType(targs[0].type);
		auto srcty = GetLLVMTypeFromResolvedType(targs[1].type);	
		CompileAssert(GetLLVMTypeSizeInBit(ty) == GetLLVMTypeSizeInBit(srcty), func->Pos, 
			string("Cannot bitcast from type ") + targs[1].type.GetString() + " to type " + targs[0].type.GetString() + ", because they have different sizes");

		return builder.CreateBitOrPointerCast(v, ty);
	}

	static void Intrinsic_UnsafeStaticCast_Phase1(FunctionAST* func)
	{
		CheckIntrinsic(func, 1, 2);
		auto& targs = *func->template_instance_args;
		CompileAssert(targs[0].kind == TemplateArgument::TEMPLATE_ARG_TYPE && targs[1].kind == TemplateArgument::TEMPLATE_ARG_TYPE,
			func->Pos, "The 1st and 2nd template arguments for unsafe.static_cast should be a type");
		CompileAssert(IsResolvedTypeClass(targs[0].type) && IsResolvedTypeClass(targs[1].type)
			&& (targs[0].type.class_ast->HasParent(targs[1].type.class_ast) || targs[1].type.class_ast->HasParent(targs[0].type.class_ast) ||
				targs[1].type.class_ast->HasImplement(targs[0].type.class_ast) || targs[0].type.class_ast->HasImplement(targs[1].type.class_ast)),
			func->Pos,
			string("Cannot static_cast from type ") + targs[1].type.GetString() + " to type " + targs[0].type.GetString());
	}

	static llvm::Value* Intrinsic_UnsafeStaticCast_Generate(FunctionAST* func, llvm::Value* obj, vector<unique_ptr<ExprAST>>& args)
	{
		assert(func->template_instance_args);
		assert(func->template_instance_args->size() == 2);
		assert(args.size() == 1);
		auto& targs = *func->template_instance_args;
		auto v = args[0]->Generate();
		auto ty = GetLLVMTypeFromResolvedType(targs[0].type);
		ClassAST* from = targs[1].type.class_ast;
		ClassAST* target = targs[0].type.class_ast;
		if (target->HasParent(from) || from->HasParent(target)) {
			return builder.CreateBitOrPointerCast(v, ty);
		}
		else if (from->HasImplement(target)) {
			// upcast to interface
			for (int i = 0; i < from->implements.size(); ++i)
			{
				ClassAST* curcls = from->implements[i];
				while (curcls && curcls != target)
				{
					curcls = curcls->parent_class;
				}
				if (curcls)
				{
					auto ret_val = builder.CreateInsertValue(llvm::UndefValue::get(ty),
						GetOrCreateTypeInfoGlobal(from, from->implements[i], from->if_vtabledef[i].size(), i), 0);
					ret_val = builder.CreateInsertValue(ret_val,
						builder.CreatePointerCast(v, target->llvm_type->getPointerTo()), 1);
					return ret_val;
				}
			}
		}
		else if (target->HasImplement(from)) {
			// downcast from interface
			return builder.CreatePointerCast(
				builder.CreateExtractValue(v, 1), target->llvm_type->getPointerTo());
		}
		// cannot reach here
		assert(false);
		return nullptr;
	}


	static void Intrinsic_RttiGetTypeInfo_Phase1(FunctionAST* func)
	{
		CheckIntrinsic(func, 0, 1);
		auto& targs = *func->template_instance_args;
		CompileAssert(targs[0].kind == TemplateArgument::TEMPLATE_ARG_TYPE,
			func->Pos, "The 1st template argument for rtti.get_type_info should be a type");
		CompileAssert(IsResolvedTypeClass(targs[0].type) && targs[0].type.class_ast->needs_rtti,
			func->Pos,
			string("Cannot get type info from class ") + targs[0].type.GetString() + " which has not enabled rtti");
	}
	
	static llvm::Value* Intrinsic_RttiGetTypeInfo_Generate(FunctionAST* func, llvm::Value* obj, vector<unique_ptr<ExprAST>>& args)
	{
		assert(func->template_instance_args);
		assert(func->template_instance_args->size() == 1);
		assert(args.size() == 0);
		auto& targs = *func->template_instance_args;
		return GetOrCreateTypeInfoGlobal(targs[0].type.class_ast);
	}


	static void Intrinsic_TypedptrGetFieldPtr_Phase1(FunctionAST* func)
	{
		CheckIntrinsic(func, 1, 2);
		auto& targs = *func->template_instance_args;
		CompileAssert(targs[0].kind == TemplateArgument::TEMPLATE_ARG_EXPR
			&& isa<StringLiteralAST>(targs[0].expr.get()),
			func->Pos, "The first template argument for unsafe.get_field_ptr should be a string");
		CompileAssert(targs[1].kind == TemplateArgument::TEMPLATE_ARG_TYPE,
			func->Pos, "The second template argument for unsafe.get_field_ptr should be a type");
		auto& rtype = targs[1].type;
		bool is_class_t = rtype.type == tok_class && rtype.index_level == 0;
		CompileAssert(is_class_t, func->Pos,
			string("Cannot get field pointer from non class type ") + rtype.GetString());
		auto& args = func->Proto->resolved_args;
		CompileAssert(args.size() == 1 && args[0]->resolved_type.index_level == 0
			&& args[0]->resolved_type.type == tok_pointer, func->Pos,
			"The argument should be pointer");
	}

	extern llvm::Value* GenerateMemberGEP(llvm::Value* llvm_obj, ClassAST* curcls, FieldDef* field, int casade_parents);

	static llvm::Value* Intrinsic_TypedptrGetFieldPtr_Generate(FunctionAST* func, llvm::Value* obj, vector<unique_ptr<ExprAST>>& args)
	{
		assert(func->template_instance_args);
		assert(func->template_instance_args->size() == 2);
		assert(args.size() == 1);
		auto& targs = *func->template_instance_args;
		string& name = static_cast<StringLiteralAST*>(targs[0].expr.get())->Val;
		ClassAST* cls = targs[1].type.class_ast;
		auto ret = FindClassField(cls, name);
		CompileAssert(ret.first >= 0, func->Pos, string("Cannot find field ") + name + " in class " + cls->GetUniqueName());
		auto v = args[0]->Generate();
		auto ty = GetLLVMTypeFromResolvedType(ResolvedType(cls));
		if (cls->is_struct)
			ty = ty->getPointerTo();
		auto pcls = builder.CreatePointerCast(v, ty);
		auto gep = GenerateMemberGEP(pcls, cls, ret.second, ret.first);
		return builder.CreatePointerCast(gep, builder.getInt8PtrTy());
	}

	static void Intrinsic_TypedptrSizeof_Phase1(FunctionAST* func)
	{
		CheckIntrinsic(func, 0, 1);
		auto& targs = *func->template_instance_args;
		CompileAssert(targs[0].kind == TemplateArgument::TEMPLATE_ARG_TYPE,
			func->Pos, "The 1st template argument for rtti.get_type_info should be a type");
	}

	static llvm::Value* Intrinsic_TypedptrSizeof_Generate(FunctionAST* func, llvm::Value* obj, vector<unique_ptr<ExprAST>>& args)
	{
		assert(func->template_instance_args);
		assert(func->template_instance_args->size() == 1);
		assert(args.size() == 0);
		auto& targs = *func->template_instance_args;
		auto ty = GetLLVMTypeFromResolvedType(targs[0].type);
		auto bigptr = builder.CreateGEP(llvm::Constant::getNullValue(ty->getPointerTo()), builder.getInt32(1));
		return builder.CreatePtrToInt(bigptr, builder.getInt64Ty());
	}


	static IntrisicFunction unsafe_bit_cast = { Intrinsic_UnsafeBitCast_Generate , Intrinsic_UnsafeBitCast_Phase1 };
	static IntrisicFunction unsafe_ptr_cast = { Intrinsic_UnsafePtrCast_Generate , Intrinsic_UnsafePtrCast_Phase1 };
	static IntrisicFunction unsafe_ptr_load = { Intrinsic_UnsafePtrLoad_Generate , Intrinsic_UnsafePtrLoad_Phase1 };
	static IntrisicFunction unsafe_ptr_store = { Intrinsic_UnsafePtrStore_Generate , Intrinsic_UnsafePtrStore_Phase1 };
	static IntrisicFunction unsafe_static_cast = { Intrinsic_UnsafeStaticCast_Generate , Intrinsic_UnsafeStaticCast_Phase1 };
	static IntrisicFunction typedptr_get_field_ptr = { Intrinsic_TypedptrGetFieldPtr_Generate , Intrinsic_TypedptrGetFieldPtr_Phase1 };
	static IntrisicFunction typedptr_sizeof = { Intrinsic_TypedptrSizeof_Generate , Intrinsic_TypedptrSizeof_Phase1 };
	static IntrisicFunction rtti_get_type_info = { Intrinsic_RttiGetTypeInfo_Generate , Intrinsic_RttiGetTypeInfo_Phase1 };

	static unordered_map<string, unordered_map<string, IntrisicFunction*>> intrinsic_module_names = {
		{"unsafe",{
			{"ptr_cast",&unsafe_ptr_cast},
			{"ptr_load",&unsafe_ptr_load},
			{"ptr_store",&unsafe_ptr_store},
			{"bit_cast",&unsafe_bit_cast},
			{"static_cast",&unsafe_static_cast},
			}
		},
		{"rtti",{
			{"get_type_info",&rtti_get_type_info},
			}
		},
		{"typedptr",{
			{"sizeof",&typedptr_sizeof},
			{"get_field_ptr",&typedptr_get_field_ptr},
			}
		},
	};
	bool IsIntrinsicModule(const string& name)
	{
		return intrinsic_module_names.find(name) != intrinsic_module_names.end();
	}

	IntrisicFunction* FindIntrinsic(FunctionAST* func)
	{
		bool _is_intrinsic;
		unordered_map<string, unordered_map<string, IntrisicFunction*>>::iterator itr1;
		if (func->Proto->prefix_idx == -1)
		{
			_is_intrinsic = cu.is_intrinsic;
			//in most cases, modules are not intrinsic. We can save a map finding here.
			if (_is_intrinsic)
				itr1 = intrinsic_module_names.find(cu.symbol_prefix.substr(0, cu.symbol_prefix.length() - 1)); //symbol_prefix has a "." at the end
		}
		else
		{
			itr1 = intrinsic_module_names.find(cu.imported_module_names[func->Proto->prefix_idx]);
			_is_intrinsic = itr1 != intrinsic_module_names.end();
		}
		if (_is_intrinsic)
		{
			unordered_map<string, IntrisicFunction*>::iterator itr2;
			auto fpos = func->Proto->Name.find('[');
			if (fpos != string::npos)
				itr2 = itr1->second.find(func->Proto->Name.substr(0, fpos));
			else
				itr2 = itr1->second.find(func->Proto->Name);
			if (itr2 != itr1->second.end())
			{
				return itr2->second;
			}
		}
		return nullptr;
	}
}
