
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/Verifier.h"

#include "CompileError.h"
#include "AST.h"
#include <cassert>
#include "CastAST.h"

using namespace llvm;
using Birdee::CompileError;
using std::unordered_map;
using namespace Birdee;

static LLVMContext context;
IRBuilder<> builder(context);

static Module* module;
static std::unique_ptr<DIBuilder> DBuilder;

static llvm::Type* ty_int=IntegerType::getInt32Ty(context);
static llvm::Type* ty_long = IntegerType::getInt64Ty(context);

struct DebugInfo {
	DICompileUnit *cu;
	std::vector<DIScope *> LexicalBlocks;
	DIBasicType* GetIntType()
	{
		DIBasicType* ret = nullptr;
		if (ret)
			return ret;
		ret = DBuilder->createBasicType("uint", 32, dwarf::DW_ATE_unsigned);
		return ret;
	}
	void emitLocation(Birdee::StatementAST *AST);
} dinfo;


namespace std
{

	template <>
	struct hash<Birdee::ResolvedType>
	{
		std::size_t operator()(const Birdee::ResolvedType& a) const
		{
			uintptr_t v = (((int)a.type) << 5) + a.index_level + (uintptr_t)a.proto_ast;
			hash<uintptr_t> has;
			return has(v);
		}
	};
}


namespace Birdee
{
	extern ClassAST* GetStringClass();
}
template<typename T>
void Print(T* v)
{
	std::string type_str;
	llvm::raw_string_ostream rso(type_str);
	v->print(rso);
	std::cout << rso.str();
}

typedef DIType* PDIType;
bool GenerateType(const Birdee::ResolvedType& type, PDIType& dtype, llvm::Type* & base);
struct LLVMHelper {
	Function* cur_llvm_func = nullptr;
	ClassAST* cur_class_ast = nullptr;

	unordered_map<Birdee::ResolvedType, llvm::Type*> typemap;
	unordered_map<Birdee::ResolvedType, DIType *> dtypemap;
	llvm::Type* GetType(const Birdee::ResolvedType& ty)
	{
		PDIType dtype;
		return  GetType(ty, dtype);
	}
	llvm::Type* GetType(const Birdee::ResolvedType& ty, PDIType& dtype)
	{
		auto itr = typemap.find(ty);
		if (itr != typemap.end())
		{
			dtype = dtypemap[ty];
			return itr->second;
		}
		llvm::Type* ret;
		bool resolved = GenerateType(ty, dtype,ret);
		if (resolved)
		{
			typemap[ty] = ret;
			dtypemap[ty] = dtype;
		}
		return ret;
	}

	DIType* GetDType(const Birdee::ResolvedType& ty)
	{
		auto itr = dtypemap.find(ty);
		if (itr != dtypemap.end())
			return itr->second;
		assert(0 && "Cannot find the DIType");
		return nullptr;
	}

} helper;

void DebugInfo::emitLocation(Birdee::StatementAST *AST) {
	DIScope *Scope;
	if (LexicalBlocks.empty())
		Scope = this->cu;
	else
		Scope = LexicalBlocks.back();
	assert(helper.cur_llvm_func->getSubprogram() == Scope);
	builder.SetCurrentDebugLocation(
		DebugLoc::get(AST->Pos.line, AST->Pos.pos, Scope));
}


llvm::Type* BuildArrayType(llvm::Type* ty, DIType* & dty,string& name,DIType* & outdtype)
{
	vector<llvm::Type*> types{llvm::Type::getInt32Ty(context),ArrayType::get(ty,0)};
	SmallVector<Metadata *, 8> dtypes{ dinfo.GetIntType(),DBuilder->createArrayType(0, 0, dty,{}) }; //fix-me: what is the last parameter for???

	name +="[]";
	DIFile *Unit = DBuilder->createFile(dinfo.cu->getFilename(),
		dinfo.cu->getDirectory());
	outdtype = DBuilder->createStructType(dinfo.cu, name, Unit, 0, 128, 64, DINode::DIFlags::FlagZero, nullptr, DBuilder->getOrCreateArray(dtypes));
	outdtype = DBuilder->createPointerType(outdtype, 64);
	return  StructType::create(context, types, name)->getPointerTo();

}

bool GenerateType(const Birdee::ResolvedType& type, PDIType& dtype, llvm::Type* & base)
{
	bool resolved = true;
	switch (type.type)
	{
	case tok_boolean:
		base = llvm::Type::getInt1Ty(context);
		dtype = DBuilder->createBasicType("boolean", 1, dwarf::DW_ATE_boolean);
		break;
	case tok_uint:
		base = llvm::Type::getInt32Ty(context);
		dtype = DBuilder->createBasicType("uint", 32, dwarf::DW_ATE_unsigned);
		break;
	case tok_int:
		base= llvm::Type::getInt32Ty(context);
		dtype = DBuilder->createBasicType("int", 32, dwarf::DW_ATE_signed);
		break;
	case tok_byte:
		base = llvm::Type::getInt8Ty(context);
		dtype = DBuilder->createBasicType("byte", 8, dwarf::DW_ATE_signed);
		break;
	case tok_long:
		base = llvm::Type::getInt64Ty(context);
		dtype = DBuilder->createBasicType("long", 64, dwarf::DW_ATE_signed);
		break;
	case tok_ulong:
		base= llvm::Type::getInt64Ty(context);
		dtype = DBuilder->createBasicType("ulong", 64, dwarf::DW_ATE_unsigned);
		break;
	case tok_float:
		base= llvm::Type::getFloatTy(context);
		dtype = DBuilder->createBasicType("float", 32, dwarf::DW_ATE_float);
		break;
	case tok_double:
		base = llvm::Type::getDoubleTy(context);
		dtype =DBuilder->createBasicType("double", 64, dwarf::DW_ATE_float);
		break;
	case tok_pointer:
		base = llvm::Type::getInt8PtrTy(context);
		dtype = DBuilder->createBasicType("pointer", 64, dwarf::DW_ATE_address);
		break;
	case tok_func:
		base = type.proto_ast->GenerateFunctionType();
		dtype = type.proto_ast->GenerateDebugType();
		break;
	case tok_void:
		base = llvm::Type::getVoidTy(context);
		dtype = DBuilder->createBasicType("void", 0, dwarf::DW_ATE_address);
		break;
	case tok_class:
		if (!type.class_ast->llvm_type)
		{
			resolved = false;
			base = StructType::create(context, type.class_ast->GetUniqueName())->getPointerTo();
			dtype = DBuilder->createPointerType(DBuilder->createUnspecifiedType(type.class_ast->GetUniqueName()),64);
		}
		else
		{
			base = type.class_ast->llvm_type->getPointerTo();
			dtype = DBuilder->createPointerType(type.class_ast->llvm_dtype, 64);
		}
		break;
	default:
		assert(0 && "Error type");
	}
	if (type.index_level > 0)
	{
		string name = dtype->getName();
		for (int i = 0; i < type.index_level; i++)
		{
			base=BuildArrayType(base, dtype, name, dtype);
		}	
	}
	return resolved;
}


void Birdee::VariableSingleDefAST::PreGenerateExternForGlobal(const string& package_name)
{
	DIType* ty;
	auto type = helper.GetType(resolved_type, ty);
	string resolved_name = package_name + '.' + name;
	GlobalVariable* v = new GlobalVariable(*module, type, false, GlobalValue::ExternalLinkage,
		nullptr, resolved_name);
	DIGlobalVariableExpression* D = DBuilder->createGlobalVariableExpression(
		dinfo.cu, resolved_name, resolved_name, dinfo.cu->getFile(), 0, ty,
		true);
	llvm_value = v;

	v->addDebugInfo(D);
}

void Birdee::VariableSingleDefAST::PreGenerateForGlobal()
{
	DIType* ty;
	auto type=helper.GetType(resolved_type, ty);
	GlobalVariable* v = new GlobalVariable(*module, type,false,GlobalValue::CommonLinkage,
		Constant::getNullValue(type),cu.symbol_prefix+name);
	DIGlobalVariableExpression* D = DBuilder->createGlobalVariableExpression(
			dinfo.cu, cu.symbol_prefix + name, cu.symbol_prefix + name, dinfo.cu->getFile(), Pos.line, ty,
			true);
	llvm_value = v;
	
	v->addDebugInfo(D); 
}

void Birdee::VariableSingleDefAST::PreGenerateForArgument(llvm::Value* init,int argno)
{
	DIType* ty;
	llvm_value = builder.CreateAlloca(helper.GetType(resolved_type, ty), nullptr, name);
	
	// Create a debug descriptor for the variable.
	DILocalVariable *D = DBuilder->createParameterVariable(
		dinfo.LexicalBlocks.back(), name, argno, dinfo.cu->getFile(), Pos.line, ty,
		true);
	
	DBuilder->insertDeclare(llvm_value, D, DBuilder->createExpression(),
		DebugLoc::get(Pos.line, Pos.pos, dinfo.LexicalBlocks.back()),
		builder.GetInsertBlock());

	builder.CreateStore(init, llvm_value);
}

llvm::Value* Birdee::VariableSingleDefAST::Generate()
{
	dinfo.emitLocation(this);
	if (!llvm_value)
	{
		DIType* ty;
		llvm_value = builder.CreateAlloca(helper.GetType(resolved_type, ty), nullptr, name);

		// Create a debug descriptor for the variable.
		DILocalVariable *D = DBuilder->createAutoVariable(dinfo.LexicalBlocks.back(), name, dinfo.cu->getFile(), Pos.line, ty,
			true);

		DBuilder->insertDeclare(llvm_value, D, DBuilder->createExpression(),
			DebugLoc::get(Pos.line, Pos.pos, dinfo.LexicalBlocks.back()),
			builder.GetInsertBlock());
	}
	if (val)
	{
		auto v = val->Generate();
		return builder.CreateStore(v, llvm_value);
	}
	else
	{
		return builder.CreateStore(Constant::getNullValue(llvm_value->getType()->getPointerElementType()), llvm_value);
	}
}

DISubprogram * PrepareFunctionDebugInfo(Function* TheFunction,DISubroutineType* type,SourcePos pos)
{

	// Create a subprogram DIE for this function.
	DIFile *Unit = DBuilder->createFile(dinfo.cu->getFilename(),
		dinfo.cu->getDirectory());
	DIScope *FContext = Unit;
	unsigned LineNo = pos.line;
	unsigned ScopeLine = LineNo;
	DISubprogram *SP = DBuilder->createFunction(
		FContext, TheFunction->getName(), StringRef(), Unit, LineNo,
		type,
		false /* external linkage */, true /* definition */, ScopeLine,
		DINode::FlagPrototyped, false);
	TheFunction->setSubprogram(SP);
	return SP;
}

//https://stackoverflow.com/a/2072890/4790873
inline bool ends_with(std::string const & value, std::string const & ending)
{
	if (ending.size() > value.size()) return false;
	return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

void Birdee::CompileUnit::InitForGenerate()
{
	//InitializeNativeTargetInfo();
	InitializeNativeTarget();
	//InitializeNativeTargetMC();
	InitializeNativeTargetAsmParser();
	InitializeNativeTargetAsmPrinter();

	module= new Module(name, context);
	DBuilder = llvm::make_unique<DIBuilder>(*module);
	dinfo.cu = DBuilder->createCompileUnit(
		dwarf::DW_LANG_C, DBuilder->createFile(filename, directory),
		"Birdee Compiler", 0, "", 0);
}


void Birdee::CompileUnit::Generate()
{
	auto TargetTriple = sys::getDefaultTargetTriple();
	module->setTargetTriple(TargetTriple);

	std::string Error;
	auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);

	// Print an error and exit if we couldn't find the requested target.
	// This generally occurs if we've forgotten to initialise the
	// TargetRegistry or we have a bogus target triple.
	if (!Target) {
		errs() << Error;
		return;
	}

	auto CPU = "generic";
	auto Features = "";

	TargetOptions opt;
	auto RM = Optional<Reloc::Model>();
	auto TheTargetMachine =
		Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);

	module->setDataLayout(TheTargetMachine->createDataLayout());

	if (ends_with(cu.targetpath, ".o"))
	{
		// Add the current debug info version into the module.
		module->addModuleFlag(Module::Warning, "Debug Info Version",
			DEBUG_METADATA_VERSION);

		// Darwin only supports dwarf2.
		if (Triple(sys::getProcessTriple()).isOSDarwin())
			module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 2);
	}
	else if (ends_with(cu.targetpath, ".obj"))
	{
		module->addModuleFlag(llvm::Module::Warning, "CodeView", 1);
	}


	//first generate the classes, as the functions may reference them
	//this will generate the LLVM types for the classes
	for (auto& cls : classmap)
	{
		cls.second.get().PreGenerate();
	}

	//generate the function objects of the function. Not the time to generate the bodies, 
	//since functions may reference each other
	for (auto& cls : classmap)
	{
		cls.second.get().PreGenerateFuncs();
	}

	for (auto& func : funcmap)
	{
		func.second.get().PreGenerate();
	}

	for (auto& dim : dimmap)
	{
		dim.second.get().PreGenerateForGlobal();
	}

	FunctionType *FT =
		FunctionType::get(llvm::Type::getVoidTy(context), false);

	Function *F = Function::Create(FT, Function::ExternalLinkage, cu.expose_main ? "main" : cu.symbol_prefix + "main", module);
	BasicBlock *BB = BasicBlock::Create(context, "entry", F);
	builder.SetInsertPoint(BB);
	//SmallVector<Metadata *, 8> dargs{ DBuilder->createBasicType("void", 0, dwarf::DW_ATE_address) };
	DISubroutineType* functy = DBuilder->createSubroutineType(DBuilder->getOrCreateTypeArray({ DBuilder->createBasicType("void", 0, dwarf::DW_ATE_address) }));
	auto dbginfo = PrepareFunctionDebugInfo(F, functy, SourcePos(1, 1));
	dinfo.LexicalBlocks.push_back(dbginfo);

	helper.cur_llvm_func = F;
	// Push the current scope.

	//check if I have initialized
	GlobalVariable* check_init = new GlobalVariable(*module, builder.getInt1Ty(), false, GlobalValue::PrivateLinkage,
		builder.getInt1(false), cu.symbol_prefix + "!init");

	BasicBlock *BT = BasicBlock::Create(context, "init", F);
	BasicBlock *BF = BasicBlock::Create(context, "no_init", F);
	builder.CreateCondBr(builder.CreateLoad(check_init), BT, BF);

	builder.SetInsertPoint(BT);
	builder.CreateRetVoid();

	builder.SetInsertPoint(BF);
	builder.CreateStore(builder.getInt1(true), check_init);

	for (auto& name : cu.imported_module_names)
	{
		Function *OtherMain = Function::Create(FT, Function::ExternalLinkage, name+".main", module);
		builder.CreateCall(OtherMain);
	}
	
	if (toplevel.size() > 0)
	{
		for (auto& stmt : toplevel)
		{
			stmt->Generate();
		}
		if (toplevel.empty() || !instance_of<ReturnAST>(toplevel.back().get()))
		{
			dinfo.emitLocation(toplevel.back().get());
			builder.CreateRetVoid();
		}
	}

	dinfo.LexicalBlocks.pop_back();

	// Finalize the debug info.
	DBuilder->finalize();

	// Print out all of the generated code.
	//module->print(errs(), nullptr);

	//verifyModule(*module);


	module->print(errs(), nullptr);

	auto Filename = cu.targetpath;
	std::error_code EC;
	raw_fd_ostream dest(Filename, EC, sys::fs::F_None);

	if (EC) {
		errs() << "Could not open file: " << EC.message();
		return;
	}

	legacy::PassManager pass;
	auto FileType = TargetMachine::CGFT_ObjectFile;

	if (TheTargetMachine->addPassesToEmitFile(pass, dest, FileType)) {
		errs() << "TheTargetMachine can't emit a file of this type";
		return;
	}

	pass.run(*module);
	dest.flush();

	outs() << "Wrote " << Filename << "\n";
	dest.flush();
	//module->print(errs(), nullptr);
}

llvm::FunctionType * Birdee::PrototypeAST::GenerateFunctionType()
{
	std::vector<llvm::Type*> args;
	DIType* dummy;
	if (cls) {
		args.push_back(cls->llvm_type->getPointerTo());
	}
	for (auto& arg : resolved_args)
	{
		llvm::Type* argty = helper.GetType(arg->resolved_type, dummy);
		args.push_back(argty);
	}
	return FunctionType::get(helper.GetType(resolved_type, dummy),args,false);
}

DIType * Birdee::PrototypeAST::GenerateDebugType()
{
	SmallVector<Metadata *, 8> dargs;
	// Add the result type.
	dargs.push_back(helper.GetDType(resolved_type));
	
	if (cls) {
		dargs.push_back(DBuilder->createPointerType(cls->llvm_dtype,64));
	}
	for (auto& arg : resolved_args)
	{
		DIType* darg = helper.GetDType(arg->resolved_type);
		dargs.push_back(darg);
	}
	return DBuilder->createSubroutineType(DBuilder->getOrCreateTypeArray(dargs));
}

void Birdee::ClassAST::PreGenerate()
{
	if (llvm_type)
		return;
	vector<llvm::Type*> types;
	SmallVector<Metadata *, 8> dtypes;

	for (auto& field : fields)
	{
		DIType* dty;
		types.push_back(helper.GetType(field.decl->resolved_type, dty));
		dtypes.push_back(dty); 
	}
	llvm_type = StructType::create(context, types, GetUniqueName());
	DIFile *Unit = DBuilder->createFile(dinfo.cu->getFilename(),
		dinfo.cu->getDirectory());
	auto size = module->getDataLayout().getTypeAllocSizeInBits(llvm_type);
	auto align = module->getDataLayout().getPrefTypeAlignment(llvm_type);
	llvm_dtype=DBuilder->createStructType(dinfo.cu, GetUniqueName(), Unit, Pos.line, size, align*8, DINode::DIFlags::FlagZero, nullptr, DBuilder->getOrCreateArray(dtypes));
	//fix-me: now the debug info is not portable
}

void Birdee::ClassAST::PreGenerateFuncs()
{
	for (auto& func : funcs)
	{
		func.decl->PreGenerate();
	}
}
DIType* Birdee::FunctionAST::PreGenerate()
{
	if (llvm_func)
		return helper.dtypemap[resolved_type];
	string prefix;
	if (Proto->cls)
		prefix = Proto->cls->GetUniqueName() + ".";
	else if (Proto->prefix_idx == -1)
		prefix = cu.symbol_prefix;
	else
		prefix = cu.imported_module_names[Proto->prefix_idx]+'.';
	llvm_func = Function::Create(Proto->GenerateFunctionType(), Function::ExternalLinkage, prefix + Proto->GetName(), module);
	DIType* ret = Proto->GenerateDebugType();
	helper.dtypemap[resolved_type] = ret;
	return ret;
}


llvm::Function* GetMallocObj()
{
	static llvm::Function* func = nullptr;
	if (!func)
	{
		auto fty = FunctionType::get(builder.getInt8PtrTy(), { builder.getInt32Ty() }, false);
		func=Function::Create(fty, Function::ExternalLinkage, "BirdeeMallocObj", module);
	}
	return func;
}

llvm::Function* GetMallocArr()
{
	static llvm::Function* func = nullptr;
	if (!func)
	{
		//base_size,dimension,[size1,size2....]
		auto fty = FunctionType::get(builder.getInt8PtrTy(), { builder.getInt32Ty(),builder.getInt32Ty()}, true);
		func = Function::Create(fty, Function::ExternalLinkage, "BirdeeMallocArr", module);
	}
	return func;
}

Value* GenerateCall(Value* func, PrototypeAST* proto, Value* obj, vector<unique_ptr<ExprAST>>& Args)
{
	vector<Value*> args;
	if (obj)
		args.push_back(obj);
	for (auto& vargs : Args)
	{
		args.push_back(vargs->Generate());
	}
	return builder.CreateCall(func, args);
}


llvm::Value * Birdee::NewExprAST::Generate()
{
	
	if (resolved_type.index_level > 0)
	{
		ResolvedType tyelement(resolved_type);
		tyelement.index_level = 0;
		auto llvm_ele_ty=helper.GetType(tyelement);
		size_t sz = module->getDataLayout().getTypeAllocSize(llvm_ele_ty);
		vector<Value*> LArgs;
		LArgs.push_back(builder.getInt32(sz));
		LArgs.push_back(builder.getInt32(args.size()));
		for (auto& arg : args)
		{
			LArgs.push_back(builder.CreateZExtOrBitCast(arg->Generate(),builder.getInt32Ty()));
		}
		auto ret= builder.CreateCall(GetMallocArr(), LArgs);
		return builder.CreatePointerCast(ret, helper.GetType(resolved_type));
	}

	assert(resolved_type.type == tok_class);
	auto llvm_ele_ty = resolved_type.class_ast->llvm_type;
	size_t sz = module->getDataLayout().getTypeAllocSize(llvm_ele_ty);
	Value* ret= builder.CreateCall(GetMallocObj(), builder.getInt32(sz));
	ret = builder.CreatePointerCast(ret, llvm_ele_ty->getPointerTo());
	if(func)
		GenerateCall(func->decl->llvm_func, func->decl->Proto.get(), ret, args);
	return ret;
}

bool Birdee::ASTBasicBlock::Generate()
{
	for (auto& stmt : body)
	{
		stmt->Generate();
	}
	if (!body.empty() && instance_of<ReturnAST>(body.back().get()))
	{
		return true;
	}
	return false;
}

llvm::Value * Birdee::ThisExprAST::Generate()
{
	dinfo.emitLocation(this);
	return helper.cur_llvm_func->args().begin();
}

llvm::Value * Birdee::FunctionAST::Generate()
{
	DISubroutineType* functy=(DISubroutineType*)PreGenerate();

	auto dbginfo = PrepareFunctionDebugInfo(llvm_func, functy, Pos);

	if (!isDeclare)
	{
		dinfo.LexicalBlocks.push_back(dbginfo);
		auto IP = builder.saveIP();
		auto func_backup = helper.cur_llvm_func;
		helper.cur_llvm_func = llvm_func;
		BasicBlock *BB = BasicBlock::Create(context, "entry", llvm_func);
		builder.SetInsertPoint(BB);

		dinfo.emitLocation(this);
		int i = 0;
		auto itr = llvm_func->args().begin();
		if (Proto->cls)
			itr++;
		for (; itr != llvm_func->args().end(); itr++)
		{
			Proto->resolved_args[i]->PreGenerateForArgument(itr, i + 1);
			i++;
		}


		bool hasret = Body.Generate();
		if (!hasret)
		{
			if (resolved_type.proto_ast->resolved_type.type == tok_void)
				builder.CreateRetVoid();
			else
				builder.CreateRet(Constant::getNullValue(llvm_func->getReturnType()));
		}
		dinfo.LexicalBlocks.pop_back();
		builder.restoreIP(IP);
		helper.cur_llvm_func = func_backup;
	}
	else if(!isImported) //if is declaration and is a c-style extern function
		llvm_func->setName(Proto->GetName());
	return llvm_func;
}

llvm::Value * Birdee::IdentifierExprAST::Generate()
{
	if (resolved_type.type == tok_package)
		return nullptr;
	return impl->Generate();
}

llvm::Value * Birdee::ResolvedFuncExprAST::Generate()
{
	dinfo.emitLocation(this);
	return def->llvm_func;
}


StructType* GetStringType()
{
	static StructType* cls_string = nullptr;
	if (cls_string)
		return cls_string;
	return GetStringClass()->llvm_type;
}

llvm::Value * Birdee::StringLiteralAST::Generate()
{
	static unordered_map<reference_wrapper<const string>, GlobalVariable*> stringpool;
	dinfo.emitLocation(this);
	auto itr = stringpool.find(Val);
	if (itr != stringpool.end())
	{
		return itr->second;
	}
	static llvm::Type* byte_arr_ty = nullptr;
	if (!byte_arr_ty)
	{
		ResolvedType ty;
		ty.type = tok_byte;
		ty.index_level = 1;
		byte_arr_ty = helper.GetType(ty);
	}
	/*
	Constant * str = ConstantDataArray::getString(context, Val);
	GlobalVariable* vstr = new GlobalVariable(*module,str->getType(), true, GlobalValue::PrivateLinkage, nullptr);
	vstr->setAlignment(1);
	vstr->setInitializer(str);
	//vstr->print(errs(), true);
	std::vector<Constant*> const_ptr_5_indices;
	ConstantInt* const_int64_6 = ConstantInt::get(context, APInt(64, 0));
	const_ptr_5_indices.push_back(const_int64_6);
	const_ptr_5_indices.push_back(const_int64_6);
	Constant* const_ptr_5 = ConstantExpr::getGetElementPtr(nullptr,vstr, const_ptr_5_indices);
	//const_ptr_5->print(errs(), true);

	Constant * obj= llvm::ConstantStruct::get(GetStringType(),{
		const_ptr_5,
		ConstantInt::get(llvm::Type::getInt32Ty(context),APInt(32,Val.length(),true))
		});*/
	

	Constant * str = ConstantDataArray::getString(context, Val);
	vector<llvm::Type*> types{ llvm::Type::getInt32Ty(context),ArrayType::get(builder.getInt8Ty(),Val.length() + 1) };
	auto cur_array_ty=  StructType::create(context, types);

	Constant * strarr = llvm::ConstantStruct::get(cur_array_ty, {
		ConstantInt::get(llvm::Type::getInt32Ty(context),APInt(32,Val.length()+1,true)),
		str
		});

	GlobalVariable* vstr = new GlobalVariable(*module, strarr->getType(), true, GlobalValue::PrivateLinkage, nullptr);
	vstr->setInitializer(strarr);
	//vstr->print(errs(), true);
	std::vector<Constant*> const_ptr_5_indices;
	ConstantInt* const_int64_6 = ConstantInt::get(context, APInt(64, 0));
	const_ptr_5_indices.push_back(const_int64_6);
	Constant* const_ptr_5 = ConstantExpr::getGetElementPtr(nullptr, vstr, const_ptr_5_indices);
	const_ptr_5 = ConstantExpr::getPointerCast(const_ptr_5, byte_arr_ty);
	//const_ptr_5->print(errs(), true);

	Constant * obj = llvm::ConstantStruct::get(GetStringType(), {
		const_ptr_5,
		ConstantInt::get(llvm::Type::getInt32Ty(context),APInt(32,Val.length(),true))
		});
	GlobalVariable* vobj = new GlobalVariable(*module, GetStringType(), true, GlobalValue::PrivateLinkage, nullptr);
	vobj->setInitializer(obj);
	stringpool[Val] = vobj;
	return vobj;
}

llvm::Value * Birdee::LocalVarExprAST::Generate()
{
	dinfo.emitLocation(this);
	return builder.CreateLoad(def->llvm_value);
}

llvm::Value * Birdee::AddressOfExprAST::Generate()
{
	dinfo.emitLocation(this);
	if(!is_address_of)
		return builder.CreateBitOrPointerCast(expr->Generate(), llvm::Type::getInt8PtrTy(context));
	auto ret=expr->GetLValue(false);
	assert(ret);
	return builder.CreateBitOrPointerCast(ret, llvm::Type::getInt8PtrTy(context));
}

llvm::Value * Birdee::VariableMultiDefAST::Generate()
{
	for (auto& v : lst)
		v->Generate();
	return nullptr;
}

Value * Birdee::NumberExprAST::Generate()
{
	dinfo.emitLocation(this);
	switch (Val.type)
	{
	case tok_byte:
		return ConstantInt::get(context, APInt(8, (uint64_t)Val.v_int, true));
		break;
	case tok_int:
		return ConstantInt::get(context, APInt(32, (uint64_t)Val.v_int, true));
		break;
	case tok_long:
		return ConstantInt::get(context, APInt(64, Val.v_ulong, true));
		break;
	case tok_uint:
		return ConstantInt::get(context, APInt(32, (uint64_t)Val.v_uint, false));
		break;
	case tok_ulong:
		return ConstantInt::get(context, APInt(64, Val.v_ulong, false));
		break;
	case tok_float:
		return ConstantFP::get(context, APFloat((float)Val.v_double));
		break;
	case tok_double:
		return ConstantFP::get(context, APFloat(Val.v_double));
		break;
	default:
		assert(0 && "Error type of constant");
	}
	return nullptr;
}


Value * Birdee::ReturnAST::Generate()
{
	dinfo.emitLocation(this);
	Value* ret = Val->Generate();
	return builder.CreateRet(ret);
}

llvm::Value * Birdee::NullExprAST::Generate()
{
	return Constant::getNullValue(helper.GetType(resolved_type));
}

llvm::Value * Birdee::IndexExprAST::GetLValue(bool checkHas)
{
	if (checkHas)
		return (llvm::Value *)1;
	dinfo.emitLocation(this);
	Value* arr = Expr->Generate();
	Value* index = Index->Generate();
	arr->print(errs(), true);
	auto ptr = builder.CreateGEP(arr, { builder.getInt32(0),builder.getInt32(1),builder.getInt32(0) });
	return builder.CreateGEP(ptr, index);
}

llvm::Value * Birdee::IndexExprAST::Generate()
{
	dinfo.emitLocation(this);
	Value* arr = Expr->Generate();
	Value* index = Index->Generate();
	arr->getType()->print(errs(), true);
	auto ptr=builder.CreateGEP(arr, {builder.getInt32(0),builder.getInt32(1),builder.getInt32(0) });
	return builder.CreateLoad(builder.CreateGEP(ptr, index));
}


llvm::Value * Birdee::CallExprAST::Generate()
{
	dinfo.emitLocation(this);
	auto func=Callee->Generate();
	assert(Callee->resolved_type.type == tok_func);
	auto proto = Callee->resolved_type.proto_ast;
	Value* obj = nullptr;
	if (proto->cls)
	{
		auto pobj = dynamic_cast<MemberExprAST*>(Callee.get());
		assert(pobj);
		obj=pobj->llvm_obj;
	}
	return GenerateCall(func, proto, obj, Args);
}
namespace Birdee
{
	extern ClassAST* GetArrayClass();
}
llvm::Value * Birdee::MemberExprAST::Generate()
{
	dinfo.emitLocation(this);
	if (kind == member_field)
	{
		llvm_obj = Obj->Generate();
		return builder.CreateLoad(builder.CreateGEP(llvm_obj, { builder.getInt32(0),builder.getInt32(field->index) }));
	}
	else if (kind == member_function)
	{
		llvm_obj = Obj->Generate();
		static ClassAST* array_cls = nullptr;
		if (!array_cls)
			array_cls = GetArrayClass();
		if (Obj->resolved_type.index_level > 0)
			llvm_obj = builder.CreatePointerCast(llvm_obj, array_cls->llvm_type->getPointerTo());
		return func->decl->llvm_func;
	}
	else if (kind == member_imported_dim)
	{
		return builder.CreateLoad(import_dim->llvm_value);
	}
	else if (kind == member_imported_function)
	{
		return import_func->llvm_func;
	}
	else if (kind == member_package)
	{
		return nullptr;
	}
	else
	{
		assert(0 && "Error kind of expression");
	}
	return nullptr;
}

void SafeBr( BasicBlock* cont)
{
	if (builder.GetInsertBlock()->getInstList().empty() || !builder.GetInsertBlock()->getInstList().back().isTerminator())
		builder.CreateBr(cont);
}

llvm::Value * Birdee::IfBlockAST::Generate()
{
	dinfo.emitLocation(this);
	
	auto bt = BasicBlock::Create(context, "if_t", helper.cur_llvm_func);
	auto bf = BasicBlock::Create(context, "if_f", helper.cur_llvm_func);
	auto cont = BasicBlock::Create(context, "cont", helper.cur_llvm_func);

	auto condv = cond->Generate();

	builder.CreateCondBr(condv,bt,bf);
	builder.SetInsertPoint(bt);
	bool hasret=iftrue.Generate();
	if(!hasret)
		SafeBr(cont);

	if (!iffalse.body.empty())
	{
		builder.SetInsertPoint(bf);
		hasret=iffalse.Generate();
		if (!hasret)
			SafeBr(cont);
	}
	builder.SetInsertPoint(cont);
	return nullptr;
}

llvm::Value * Birdee::ClassAST::Generate()
{
	PreGenerate();
	for (auto& func : funcs)
	{
		func.decl->Generate();
	}
	return nullptr;
}

llvm::Value * Birdee::MemberExprAST::GetLValue(bool checkHas)
{
	if (checkHas)
	{
		return (llvm::Value *)1;
	}
	dinfo.emitLocation(this);
	llvm_obj = Obj->Generate();
	if(kind==member_field)
		return builder.CreateGEP(llvm_obj, { builder.getInt32(0),builder.getInt32(field->index) });
	return nullptr;
}

llvm::Value * BinaryGenerateFloat(Token op,Value* lv,Value* rv)
{
	switch (op)
	{
	case tok_add:
		return builder.CreateFAdd(lv, rv);
	case tok_equal:
		return builder.CreateFCmpOEQ(lv, rv);
	case tok_ne:
		return builder.CreateFCmpONE(lv, rv);
	case tok_ge:
		return builder.CreateFCmpOGE(lv, rv);
	case tok_le:
		return builder.CreateFCmpOLE(lv, rv);
	case tok_gt:
		return builder.CreateFCmpOGT(lv, rv);
	case tok_lt:
		return builder.CreateFCmpOLT(lv, rv);
	default:
		assert(0 && "Operator not supported");
		/*			tok_minus,
		tok_mul,
		tok_div,
		tok_mod,
		tok_assign,
		tok_equal,
		tok_ne,
		tok_cmp_equal,
		tok_ge,
		tok_le,
		tok_logic_and,
		tok_logic_or,
		tok_gt,
		tok_lt,
		tok_and,
		tok_or,
		tok_not,
		tok_xor,*/
	}
	return nullptr;
}


llvm::Value * BinaryGenerateInt(Token op, Value* lv, Value* rv,bool issigned)
{
	switch (op)
	{
	case tok_add:
		return builder.CreateAdd(lv, rv);
	case tok_equal:
		return builder.CreateICmpEQ(lv, rv);
	case tok_ne:
		return builder.CreateICmpNE(lv, rv);
	case tok_ge:
		return issigned ? builder.CreateICmpSGE(lv, rv): builder.CreateICmpUGE(lv, rv);
	case tok_le:
		return issigned ? builder.CreateICmpSLE(lv, rv) : builder.CreateICmpULE(lv, rv);
	case tok_gt:
		return issigned ? builder.CreateICmpSGT(lv, rv) : builder.CreateICmpUGT(lv, rv);
	case tok_lt:
		return issigned ? builder.CreateICmpSLT(lv, rv) : builder.CreateICmpULT(lv, rv);
	default:
		assert(0 && "Operator not supported");
		/*			tok_minus,
		tok_mul,
		tok_div,
		tok_mod,
		tok_assign,
		tok_equal,
		tok_ne,
		tok_cmp_equal,
		tok_ge,
		tok_le,
		tok_logic_and,
		tok_logic_or,
		tok_gt,
		tok_lt,
		tok_and,
		tok_or,
		tok_not,
		tok_xor,*/
	}
	return nullptr;
}


llvm::Value * Birdee::BinaryExprAST::Generate()
{
	dinfo.emitLocation(this);
	if (Op == tok_assign)
	{
		Value* lv = LHS->GetLValue(false);
		assert(lv);
		builder.CreateStore(RHS->Generate(), lv);
		return nullptr;
	}
	assert(LHS->resolved_type.isNumber() && RHS->resolved_type.isNumber());
	if (LHS->resolved_type.isInteger())
	{
		return BinaryGenerateInt(Op, LHS->Generate(), RHS->Generate(), LHS->resolved_type.isSigned());
	}
	else
	{
		return BinaryGenerateFloat(Op, LHS->Generate(), RHS->Generate());
	}
	return nullptr;
}
