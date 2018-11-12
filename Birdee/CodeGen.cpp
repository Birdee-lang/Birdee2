
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
#include "BdAST.h"
#include <cassert>
#include "CastAST.h"

using namespace llvm;
using Birdee::CompileError;
using std::unordered_map;
using namespace Birdee;


//https://stackoverflow.com/a/874160/4790873
bool strHasEnding(std::string const &fullString, std::string const &ending) {
	if (fullString.length() >= ending.length()) {
		return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
	}
	else {
		return false;
	}
}


std::size_t Birdee::ResolvedType::rawhash() const
{
	uintptr_t v = (((int)type) << 5) + index_level;
	if (type != tok_func)
		v += (uintptr_t)class_ast;
	else
	{
		PrototypeAST* proto = proto_ast;
		v ^= proto->resolved_type.rawhash() << 3; //return type
		v ^= (uintptr_t)proto->cls; //belonging class
		int offset = 6;
		for (auto& arg : proto->resolved_args) //argument types
		{
			v ^= arg->resolved_type.rawhash() << offset;
			offset = (offset + 3) % 32;
		}
	}
	return v;
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
void GenerateType(const Birdee::ResolvedType& type, PDIType& dtype, llvm::Type* & base);
struct LLVMHelper {
	Function* cur_llvm_func = nullptr;
	ClassAST* cur_class_ast = nullptr;
	struct LoopInfo
	{
		BasicBlock* next;
		BasicBlock* cont;
		bool isNotNull() { return next && cont; }
	}cur_loop{ nullptr,nullptr };

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
		GenerateType(ty, dtype,ret);
		typemap[ty] = ret;
		dtypemap[ty] = dtype;
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

} ;

static DIBuilder* GetDBuilder();
struct DebugInfo {
	DICompileUnit *cu;
	std::vector<DIScope *> LexicalBlocks;
	DIBasicType* GetIntType()
	{
		DIBasicType* ret = nullptr;
		if (ret)
			return ret;
		ret = GetDBuilder()->createBasicType("uint", 32, dwarf::DW_ATE_unsigned);
		return ret;
	}
	void emitLocation(Birdee::StatementAST *AST);
};

extern IRBuilder<> builder;
struct GeneratorContext
{
	LLVMContext context;
	Module* module = nullptr;
	std::unique_ptr<DIBuilder> DBuilder = nullptr;

	llvm::Type* ty_int;
	llvm::Type* ty_long;

	GeneratorContext()
	{
		ty_int = IntegerType::getInt32Ty(context);
		ty_long = IntegerType::getInt64Ty(context);
		builder.~IRBuilder<>();
		new (&builder)IRBuilder<>(context);
	}

	~GeneratorContext()
	{
		delete module;
	}
	Function* malloc_obj_func = nullptr;
	Function* malloc_arr_func = nullptr;
	StructType* cls_string = nullptr;
	DebugInfo dinfo;
	unordered_map<reference_wrapper<const string>, GlobalVariable*> stringpool;
	llvm::Type* byte_arr_ty = nullptr;
	ClassAST* array_cls = nullptr;
	LLVMHelper helper;
};
static GeneratorContext gen_context;
IRBuilder<> builder(gen_context.context); //a bug? cannot put this into GeneratorContext

DIBuilder* GetDBuilder()
{
	return gen_context.DBuilder.get();
}
#define context (gen_context.context)
//#define builder (gen_context.builder)
#define helper (gen_context.helper)
#define module (gen_context.module)
#define DBuilder (gen_context.DBuilder)
#define ty_int (gen_context.ty_int)
#define ty_long (gen_context.ty_long)
#define dinfo  (gen_context.dinfo)


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

void GenerateType(const Birdee::ResolvedType& type, PDIType& dtype, llvm::Type* & base)
{
	if (type.index_level > 0)
	{
		llvm::Type* mybase;
		PDIType mydtype;
		Birdee::ResolvedType subtype = type;
		subtype.index_level--;
		mybase = helper.GetType(subtype, mydtype);
		string name;
		if (type.index_level > 1 || type.type == tok_class)
		{ //bypass a "bug" in llvm: cannot getName from debugtype for some long struct name
			assert(mybase->getPointerElementType()->isStructTy());
			name = static_cast<StructType*>(mybase->getPointerElementType())->getStructName();
		}
		else
			name= mydtype->getName();
		base = BuildArrayType(mybase, mydtype, name, dtype);
		return;
	}
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
		base = type.proto_ast->GenerateFunctionType()->getPointerTo();
		dtype = type.proto_ast->GenerateDebugType();
		break;
	case tok_void:
		base = llvm::Type::getVoidTy(context);
		dtype = DBuilder->createBasicType("void", 0, dwarf::DW_ATE_address);
		break;
	case tok_class:
		if (!type.class_ast->llvm_type)
		{
			string name = type.class_ast->GetUniqueName();
			base = StructType::create(context, name)->getPointerTo();
			dtype = DBuilder->createPointerType(DBuilder->createUnspecifiedType(name),64);
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
}

Value * Birdee::BasicTypeExprAST::Generate()
{
	assert(0 && "Should not generate BasicTypeExprAST");
	return nullptr;
}

llvm::Value * Birdee::FunctionTemplateInstanceExprAST::Generate()
{
	dinfo.emitLocation(this);
	expr->Generate();
	return instance->llvm_func;
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
		dinfo.emitLocation(this);
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

	module = new Module(name, context);
	DBuilder = llvm::make_unique<DIBuilder>(*module);
	dinfo.cu = DBuilder->createCompileUnit(
		dwarf::DW_LANG_C, DBuilder->createFile(filename, directory),
		"Birdee Compiler", 0, "", 0);
}

void Birdee::CompileUnit::AbortGenerate()
{
	gen_context.~GeneratorContext();
	new (&gen_context) GeneratorContext();
	cu.InitForGenerate();
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

	for (auto cls : imported_class_templates)
	{
		cls->PreGenerate();
	}

	for (auto cls : imported_class_templates)
	{
		cls->PreGenerateFuncs();
	}
	for (auto func : imported_func_templates)
	{
		func->PreGenerate();
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
	auto dbginfo = PrepareFunctionDebugInfo(F, functy, SourcePos(0,1, 1));
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
		builder.SetCurrentDebugLocation(DebugLoc::get(0, 0, F->getSubprogram()));
		builder.CreateCall(OtherMain);
	}
	
	if (toplevel.size() > 0)
	{
		for (auto& stmt : toplevel)
		{
			stmt->Generate();
		}
		if (!instance_of<ReturnAST>(toplevel.back().get()))
		{
			dinfo.emitLocation(toplevel.back().get());
			builder.CreateRetVoid();
		}
	}
	else
	{
		builder.CreateRetVoid();
	}

	dinfo.LexicalBlocks.pop_back();

	for (auto cls : imported_class_templates)
	{
		cls->Generate();
	}

	for (auto func : imported_func_templates)
	{
		func->Generate();
	}



	// Finalize the debug info.
	DBuilder->finalize();

	// Print out all of the generated code.
	//module->print(errs(), nullptr);

	//verifyModule(*module);

	if(cu.is_print_ir)
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

Value * Birdee::ScriptAST::Generate()
{
	return stmt ? stmt->Generate():nullptr;
}

void Birdee::ClassAST::PreGenerate()
{
	if (isTemplate())
	{
		assert(template_param && "template_param should not be null");
		for (auto& v : template_param->instances)
		{
			v.second->PreGenerate();
		}
		return;
	}
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
	llvm_type = (llvm::StructType*) helper.GetType(ResolvedType(this))->getPointerElementType();//StructType::create(context, types, GetUniqueName());
	llvm_type->setBody(types);
	DIFile *Unit = DBuilder->createFile(dinfo.cu->getFilename(),
		dinfo.cu->getDirectory());
	auto size = module->getDataLayout().getTypeAllocSizeInBits(llvm_type);
	auto align = module->getDataLayout().getPrefTypeAlignment(llvm_type);
	llvm_dtype=DBuilder->createStructType(dinfo.cu, GetUniqueName(), Unit, Pos.line, size, align*8, DINode::DIFlags::FlagZero, nullptr, DBuilder->getOrCreateArray(dtypes));
	//fix-me: now the debug info is not portable
}

void Birdee::ClassAST::PreGenerateFuncs()
{
	if (isTemplate())
	{
		for (auto& v : template_param->instances)
		{
			v.second->PreGenerateFuncs();
		}
		return;
	}
	for (auto& func : funcs)
	{
		func.decl->PreGenerate();
	}
}
DIType* Birdee::FunctionAST::PreGenerate()
{
	if (isTemplate())
	{
		assert(template_param && "template_param should not be null");
		for (auto& v : template_param->instances)
		{
			v.second->PreGenerate();
		}
		return nullptr;
	}
	if (llvm_func)
		return helper.dtypemap[resolved_type];
	string prefix;
	if (Proto->cls)
		prefix = Proto->cls->GetUniqueName() + ".";
	else if (Proto->prefix_idx == -1)
		prefix = cu.symbol_prefix;
	else
		prefix = cu.imported_module_names[Proto->prefix_idx]+'.';
	GlobalValue::LinkageTypes linkage;
	if (Proto->cls && Proto->cls->isTemplateInstance() || isTemplateInstance)
		linkage = Function::LinkOnceODRLinkage;
	else
		linkage = Function::ExternalLinkage;
	llvm_func = Function::Create(Proto->GenerateFunctionType(), linkage, prefix + Proto->GetName(), module);
	if (strHasEnding(cu.targetpath,".obj") && (Proto->cls && Proto->cls->isTemplateInstance() || isTemplateInstance))
	{
		llvm_func->setComdat(module->getOrInsertComdat(llvm_func->getName()));
		llvm_func->setDSOLocal(true);
	}
	DIType* ret = Proto->GenerateDebugType();
	helper.dtypemap[resolved_type] = ret;
	return ret;
}


llvm::Function* GetMallocObj()
{
	if (!gen_context.malloc_obj_func)
	{
		auto fty = FunctionType::get(builder.getInt8PtrTy(), { builder.getInt32Ty(),builder.getInt8PtrTy() }, false);
		gen_context.malloc_obj_func = Function::Create(fty, Function::ExternalLinkage, "BirdeeMallocObj", module);
	}
	return gen_context.malloc_obj_func;
}

llvm::Function* GetMallocArr()
{
	if (!gen_context.malloc_arr_func)
	{
		//base_size,dimension,[size1,size2....]
		auto fty = FunctionType::get(builder.getInt8PtrTy(), { builder.getInt32Ty(),builder.getInt32Ty()}, true);
		gen_context.malloc_arr_func = Function::Create(fty, Function::ExternalLinkage, "BirdeeMallocArr", module);
	}
	return gen_context.malloc_arr_func;
}

Value* GenerateCall(Value* func, PrototypeAST* proto, Value* obj, const vector<unique_ptr<ExprAST>>& Args,SourcePos pos)
{
	vector<Value*> args;
	if (obj)
	{
		args.push_back(obj);
	}
	for (auto& vargs : Args)
	{
		args.push_back(vargs->Generate());
	}
	builder.SetCurrentDebugLocation(
		DebugLoc::get(pos.line, pos.pos, helper.cur_llvm_func->getSubprogram()));
	return builder.CreateCall(func, args);
}


llvm::Value * Birdee::NewExprAST::Generate()
{
	
	if (resolved_type.index_level > 0)
	{
		ResolvedType tyelement(resolved_type);
		tyelement.index_level -= args.size();
		assert(tyelement.index_level >= 0);
		auto llvm_ele_ty=helper.GetType(tyelement);
		size_t sz = module->getDataLayout().getTypeAllocSize(llvm_ele_ty);
		vector<Value*> LArgs;
		LArgs.push_back(builder.getInt32(sz));
		LArgs.push_back(builder.getInt32(args.size()));
		for (auto& arg : args)
		{
			LArgs.push_back(builder.CreateZExtOrBitCast(arg->Generate(),builder.getInt32Ty()));
		}
		dinfo.emitLocation(this);
		auto ret= builder.CreateCall(GetMallocArr(), LArgs);
		return builder.CreatePointerCast(ret, helper.GetType(resolved_type));
	}

	assert(resolved_type.type == tok_class);
	auto llvm_ele_ty = resolved_type.class_ast->llvm_type;
	size_t sz = module->getDataLayout().getTypeAllocSize(llvm_ele_ty);
	dinfo.emitLocation(this);
	string del_func = "__del__";
	auto class_ast = resolved_type.class_ast;
	auto itr = class_ast->funcmap.find(del_func);
	Value* finalizer;
	if (itr != class_ast->funcmap.end())
		finalizer = builder.CreatePointerCast(class_ast->funcs[itr->second].decl->llvm_func, builder.getInt8PtrTy());
	else
		finalizer = Constant::getNullValue(builder.getInt8PtrTy());
	Value* ret = builder.CreateCall(GetMallocObj(), { builder.getInt32(sz), finalizer });
	ret = builder.CreatePointerCast(ret, llvm_ele_ty->getPointerTo());
	if(func)
		GenerateCall(func->decl->llvm_func, func->decl->Proto.get(), ret, args,this->Pos);
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
	if (isTemplate())
	{
		for (auto& v : template_param->instances)
		{
			v.second->Generate();
		}
		return nullptr;
	}
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
	else if (!isImported) //if is declaration and is a c-style extern function
	{
		if(link_name.empty())
			llvm_func->setName(Proto->GetName());
		else
			llvm_func->setName(link_name);
	}
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


llvm::Value * Birdee::LoopControlAST::Generate()
{
	if (!helper.cur_loop.isNotNull())
		throw CompileError(Pos.line, Pos.pos, "continue or break cannot be used outside of a loop");
	if (tok == tok_break)
		builder.CreateBr(helper.cur_loop.cont);
	else if (tok == tok_continue)
		builder.CreateBr(helper.cur_loop.next);
	else
		abort();
	return nullptr;
}

StructType* GetStringType()
{
	if (gen_context.cls_string)
		return gen_context.cls_string;
	return GetStringClass()->llvm_type;
}

llvm::Value * Birdee::BoolLiteralExprAST::Generate()
{
	return builder.getInt1(v);
}

llvm::Value * Birdee::StringLiteralAST::Generate()
{
	dinfo.emitLocation(this);
	auto itr = gen_context.stringpool.find(Val);
	if (itr != gen_context.stringpool.end())
	{
		return itr->second;
	}
	if (!gen_context.byte_arr_ty)
	{
		ResolvedType ty;
		ty.type = tok_byte;
		ty.index_level = 1;
		gen_context.byte_arr_ty = helper.GetType(ty);
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
	const_ptr_5 = ConstantExpr::getPointerCast(const_ptr_5, gen_context.byte_arr_ty);
	//const_ptr_5->print(errs(), true);

	Constant * obj = llvm::ConstantStruct::get(GetStringType(), {
		const_ptr_5,
		ConstantInt::get(llvm::Type::getInt32Ty(context),APInt(32,Val.length(),true))
		});
	GlobalVariable* vobj = new GlobalVariable(*module, GetStringType(), true, GlobalValue::PrivateLinkage, nullptr);
	vobj->setInitializer(obj);
	gen_context.stringpool[Val] = vobj;
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
		return ( isTemplateInstance()?nullptr: (llvm::Value *)1 );
	dinfo.emitLocation(this);
	Value* arr = Expr->Generate();
	Value* index = Index->Generate();
	auto ptr = builder.CreateGEP(arr, { builder.getInt32(0),builder.getInt32(1),builder.getInt32(0) });
	return builder.CreateGEP(ptr, index);
}

llvm::Value * Birdee::IndexExprAST::Generate()
{
	if (instance)
	{
		return instance->Generate();
	}
	dinfo.emitLocation(this);
	Value* arr = Expr->Generate();
	Value* index = Index->Generate();
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
		if(pobj)
			obj=pobj->llvm_obj;
		else
		{
			auto piden= dynamic_cast<IdentifierExprAST*>(Callee.get());
			if (piden)
			{
				assert(isa<MemberExprAST>(piden->impl.get()));
				obj = dynamic_cast<MemberExprAST*>(piden->impl.get())->llvm_obj;
			}
			else
			{
				auto pidx = dynamic_cast<IndexExprAST*>(Callee.get());
				assert(pidx);
				pobj = dynamic_cast<MemberExprAST*>(pidx->instance->expr.get());
				assert(pobj);
				obj = pobj->llvm_obj;
			}
		}
	}
	return GenerateCall(func, proto, obj, Args,this->Pos);
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
		if (!gen_context.array_cls)
			gen_context.array_cls = GetArrayClass();
		if (Obj->resolved_type.index_level > 0)
			llvm_obj = builder.CreatePointerCast(llvm_obj, gen_context.array_cls->llvm_type->getPointerTo());
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
	
	auto cont = BasicBlock::Create(context, "cont", helper.cur_llvm_func);
	BasicBlock* bf = iffalse.body.empty()? nullptr:BasicBlock::Create(context, "if_f", helper.cur_llvm_func,cont);
	auto bt = BasicBlock::Create(context, "if_t", helper.cur_llvm_func,bf);

	auto condv = cond->Generate();

	builder.CreateCondBr(condv,bt, iffalse.body.empty()?cont:bf);
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


llvm::Value * Birdee::ForBlockAST::Generate()
{
	dinfo.emitLocation(this);
	auto loopinfo = helper.cur_loop;
	init->Generate();
	Value* loopvar;
	bool issigned;
	if (isdim)
	{
		auto var = (VariableSingleDefAST*)init.get();
		issigned=var->resolved_type.isSigned();
		loopvar = var->llvm_value;
	}
	else
	{
		issigned = loop_var->resolved_type.isSigned();
		loopvar = loop_var->GetLValue(false);
	}
	
	auto cont = BasicBlock::Create(context, "cont", helper.cur_llvm_func);
	auto inc = BasicBlock::Create(context, "for_inc", helper.cur_llvm_func, cont);
	auto bt = BasicBlock::Create(context, "for", helper.cur_llvm_func,inc);
	auto check = BasicBlock::Create(context, "forcheck", helper.cur_llvm_func,bt);
	
	helper.cur_loop.next = inc;
	helper.cur_loop.cont = cont;

	builder.CreateBr(check);
	builder.SetInsertPoint(check);
	Value* cond;
	auto v = builder.CreateLoad(loopvar);
	if (issigned)
	{
		if (including)
			cond = builder.CreateICmpSLE(v, till->Generate());
		else
			cond = builder.CreateICmpSLT(v, till->Generate());
	}
	else
	{
		if (including)
			cond = builder.CreateICmpULE(v, till->Generate());
		else
			cond = builder.CreateICmpULT(v, till->Generate());
	}
	builder.CreateCondBr(cond, bt, cont);

	builder.SetInsertPoint(bt);
	bool hasret = block.Generate();
	if (!hasret)
	{
		SafeBr(inc);
	}

	builder.SetInsertPoint(inc);
	Value* newloopv = builder.CreateLoad(loopvar);
	builder.CreateStore(
			builder.CreateAdd(newloopv, ConstantInt::get(newloopv->getType(), 1)),
			loopvar);
		//do loopvar++
	SafeBr(check);
	

	builder.SetInsertPoint(cont);
	helper.cur_loop = loopinfo;
	return nullptr;
}

llvm::Value * Birdee::WhileBlockAST::Generate()
{
	dinfo.emitLocation(this);
	auto loopinfo = helper.cur_loop;

	auto cont = BasicBlock::Create(context, "cont", helper.cur_llvm_func);
	auto bt = BasicBlock::Create(context, "while_do", helper.cur_llvm_func, cont);
	auto check = BasicBlock::Create(context, "while_check", helper.cur_llvm_func, bt);

	helper.cur_loop.next = check;
	helper.cur_loop.cont = cont;

	builder.CreateBr(check);
	builder.SetInsertPoint(check);

	auto v = cond->Generate();
	builder.CreateCondBr(v, bt, cont);

	builder.SetInsertPoint(bt);
	bool hasret = block.Generate();
	if (!hasret)
	{
		SafeBr(check);
	}

	builder.SetInsertPoint(cont);
	helper.cur_loop = loopinfo;
	return nullptr;
}

llvm::Value * Birdee::ClassAST::Generate()
{
	PreGenerate();
	if (isTemplate())
	{
		assert(template_param && "template_param should not be null");
		for (auto& v : template_param->instances)
		{
			v.second->Generate();
		}
		return nullptr;
	}
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
	case tok_minus:
		return builder.CreateFSub(lv, rv);
	case tok_equal:
	case tok_cmp_equal:
		return builder.CreateFCmpOEQ(lv, rv);
	case tok_ne:
	case tok_cmp_ne:
		return builder.CreateFCmpONE(lv, rv);
	case tok_ge:
		return builder.CreateFCmpOGE(lv, rv);
	case tok_le:
		return builder.CreateFCmpOLE(lv, rv);
	case tok_gt:
		return builder.CreateFCmpOGT(lv, rv);
	case tok_lt:
		return builder.CreateFCmpOLT(lv, rv);
	case tok_mul:
		return builder.CreateFMul(lv, rv);
	case tok_div:
		return builder.CreateFDiv(lv, rv);
	case tok_mod:
		return builder.CreateFRem(lv, rv);
	default:
		assert(0 && "Operator not supported");
	}
	return nullptr;
}


llvm::Value * BinaryGenerateInt(Token op, Value* lv, Value* rv,bool issigned)
{
	switch (op)
	{
	case tok_add:
		return builder.CreateAdd(lv, rv);
	case tok_minus:
		return builder.CreateSub(lv, rv);
	case tok_equal:
	case tok_cmp_equal:
		return builder.CreateICmpEQ(lv, rv);
	case tok_ne:
	case tok_cmp_ne:
		return builder.CreateICmpNE(lv, rv);
	case tok_ge:
		return issigned ? builder.CreateICmpSGE(lv, rv): builder.CreateICmpUGE(lv, rv);
	case tok_le:
		return issigned ? builder.CreateICmpSLE(lv, rv) : builder.CreateICmpULE(lv, rv);
	case tok_gt:
		return issigned ? builder.CreateICmpSGT(lv, rv) : builder.CreateICmpUGT(lv, rv);
	case tok_lt:
		return issigned ? builder.CreateICmpSLT(lv, rv) : builder.CreateICmpULT(lv, rv);
	case tok_mod:
		return issigned ? builder.CreateSRem(lv, rv) : builder.CreateURem(lv, rv);
	case tok_mul:
		return builder.CreateMul(lv, rv);
	case tok_div:
		return issigned ? builder.CreateSDiv(lv, rv) : builder.CreateUDiv(lv, rv);
	default:
		assert(0 && "Operator not supported");
	}
	return nullptr;
}


llvm::Value * BinaryGenerateBool(Token op, ExprAST* lvexpr, ExprAST* rvexpr)
{
	auto lv = lvexpr->Generate();
	switch (op)
	{
	case tok_logic_or:
	{
		BasicBlock* logic_false = BasicBlock::Create(context, "or_f", helper.cur_llvm_func);
		BasicBlock* logic_conti = BasicBlock::Create(context, "or_conti", helper.cur_llvm_func);
		BasicBlock* if_f = BasicBlock::Create(context, "or_second_expr", helper.cur_llvm_func, logic_false);
		BasicBlock* oldbb = builder.GetInsertBlock();

		builder.CreateCondBr(lv, logic_conti, if_f);

		builder.SetInsertPoint(logic_false);
		builder.CreateBr(logic_conti);

		builder.SetInsertPoint(if_f);
		auto rv = rvexpr->Generate();
		BasicBlock* bif_f = builder.GetInsertBlock();
		builder.CreateCondBr(rv, logic_conti, logic_false);

		builder.SetInsertPoint(logic_conti);

		logic_conti = nullptr;
		PHINode* v2= builder.CreatePHI(builder.getInt1Ty(), 3);
		v2->addIncoming(builder.getInt1(true), oldbb); //if lv is true
		v2->addIncoming(builder.getInt1(false), logic_false); //if rv is false
		v2->addIncoming(builder.getInt1(true), bif_f); //if rv is true
		return v2;
		break;
	}
	case tok_logic_and:
	{
		BasicBlock* logic_true = BasicBlock::Create(context, "and_t", helper.cur_llvm_func);
		BasicBlock* logic_conti = BasicBlock::Create(context, "and_conti", helper.cur_llvm_func);
		BasicBlock* if_t = BasicBlock::Create(context, "and_second_expr", helper.cur_llvm_func, logic_true);
		BasicBlock* oldbb = builder.GetInsertBlock();
		builder.CreateCondBr(lv, if_t, logic_conti);

		builder.SetInsertPoint(logic_true);
		builder.CreateBr(logic_conti);

		builder.SetInsertPoint(if_t);
		auto rv = rvexpr->Generate();
		BasicBlock* bif_t = builder.GetInsertBlock();
		builder.CreateCondBr(rv, logic_true, logic_conti);

		builder.SetInsertPoint(logic_conti);

		logic_conti = nullptr;
		PHINode* v = builder.CreatePHI(builder.getInt1Ty(), 3);
		v->addIncoming(builder.getInt1(false), oldbb); //if lv is false
		v->addIncoming(builder.getInt1(true), logic_true); //if both true
		v->addIncoming(builder.getInt1(false), bif_t); //if rv is false
		return v;
	}
	case tok_and:
		return builder.CreateAnd(lv, rvexpr->Generate());
	case tok_or:
		return builder.CreateOr(lv, rvexpr->Generate());
	case tok_xor:
		return builder.CreateXor(lv, rvexpr->Generate());
	default:
		assert(0 && "Operator not supported");
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
		auto rv = RHS->Generate();
		dinfo.emitLocation(this);
		builder.CreateStore(rv, lv);
		return nullptr;
	}
	if (LHS->resolved_type.isReference())
	{
		if (Op == tok_cmp_equal)
		{
			return builder.CreateICmpEQ(builder.CreatePtrToInt(LHS->Generate(), builder.getInt64Ty()),
				builder.CreatePtrToInt(RHS->Generate(), builder.getInt64Ty()));
		}
		if (Op == tok_cmp_ne)
		{
			return builder.CreateICmpNE(builder.CreatePtrToInt(LHS->Generate(), builder.getInt64Ty()),
				builder.CreatePtrToInt(RHS->Generate(), builder.getInt64Ty()));
		}
		if(LHS->resolved_type.type == tok_class && LHS->resolved_type.index_level == 0)
		{
			assert(func);
			auto proto = func->resolved_type.proto_ast;
			vector<unique_ptr<ExprAST>> vec; vec.push_back(std::move(RHS));
			return GenerateCall(func->llvm_func, proto, LHS->Generate(), vec,this->Pos);
		}
	}
	if (isLogicToken(Op)) //boolean
	{
		return BinaryGenerateBool(Op, LHS.get(), RHS.get());
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

Value * Birdee::AnnotationStatementAST::Generate()
{
	return impl->Generate();
}