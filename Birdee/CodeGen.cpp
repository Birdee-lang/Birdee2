
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
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
#include "NameMangling.h"

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
		v ^= proto_ast->rawhash();
	return v;
}

namespace Birdee
{
	extern ClassAST* GetStringClass();
	extern ClassAST* GetTypeInfoClass();
}

template<typename T>
void Print(T* v)
{
	std::string type_str;
	llvm::raw_string_ostream rso(type_str);
	v->print(rso);
	std::cout << rso.str();
}

struct StringRefOrHolder
{
private:
	const string* pstr = nullptr;
	string str;
public:
	const string& get() const
	{
		if (pstr)
			return *pstr;
		return str;
	}
	bool operator ==(const StringRefOrHolder& other) const
	{
		return get() == other.get();
	}
	StringRefOrHolder(const string* pstr):pstr(pstr) {}
	StringRefOrHolder(const string& str) :str(str) {}
	StringRefOrHolder() {}
};

namespace std
{
	template <>
	struct hash<StringRefOrHolder>
	{
		std::size_t operator()(const StringRefOrHolder& a) const
		{
			return hash<string>()(a.get());
		}
	};
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

	struct TypePair
	{
		llvm::Type* llvm_ty=nullptr;
		DIType * dty=nullptr;
	};
	unordered_map<Birdee::ResolvedType, TypePair> typemap;
	llvm::Type* GetType(const Birdee::ResolvedType& ty)
	{
		return  GetTypeNode(ty).llvm_ty;
	}
	TypePair& GetTypeNode(const Birdee::ResolvedType& ty)
	{
		auto itr = typemap.find(ty);
		if (itr != typemap.end())
		{
			return itr->second;
		}
		llvm::Type* ret;
		DIType* dtype;
		GenerateType(ty, dtype,ret);
		typemap[ty] = { ret,dtype };
		return typemap[ty];
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
	string mangled_symbol_prefix;

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
	Function* personality_func = nullptr;
	Function* begin_catch_func = nullptr;
	Function* throw_func = nullptr;
	StructType* cls_string = nullptr;
	ClassAST* cls_typeinfo = nullptr;
	DebugInfo dinfo;
	unordered_map<StringRefOrHolder, GlobalVariable*> stringpool;
	llvm::Type* byte_arr_ty = nullptr;
	ClassAST* array_cls = nullptr;
	LLVMHelper helper;
	FunctionAST* cur_func = nullptr;
	//if landingpad!=null, we are in a try-block
	llvm::BasicBlock* landingpad = nullptr;
	StructType* landingpadtype = nullptr;
	unordered_map<ClassAST*, GlobalVariable*> rtti_map;

	//the wrappers for raw functions, useful for conversion from functype to closure
	unordered_map<Function*, Function*> function_wrapper_cache;

	//the wrappers for raw functions pointers, useful for conversion from functype to closure
	unordered_map<std::reference_wrapper<PrototypeAST>, Function*> function_ptr_wrapper_cache;

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


namespace Birdee
{
	llvm::Type* GetLLVMTypeFromResolvedType(const ResolvedType& ty)
	{
		return helper.GetType(ty);
	}

	int GetLLVMTypeSizeInBit(llvm::Type* ty)
	{
		return module->getDataLayout().getTypeAllocSizeInBits(ty);
	}
}

BD_CORE_API int GetTypeSize(ResolvedType ty)
{
	if (ty.type == tok_class && ty.class_ast->is_struct)
	{
		throw CompileError("Cannot get size of a struct");
	}
	return GetLLVMTypeSizeInBit(GetLLVMTypeFromResolvedType(ty)) / 8;
}

static ClassAST* GetTypeInfoType()
{
	if (gen_context.cls_typeinfo)
		return gen_context.cls_typeinfo;
	gen_context.cls_typeinfo = GetTypeInfoClass();
	return gen_context.cls_typeinfo;
}

static GlobalVariable* GetOrCreateTypeInfoGlobal(ClassAST* cls)
{
	auto itr = gen_context.rtti_map.find(cls);
	if (itr != gen_context.rtti_map.end())
	{
		return itr->second;
	}
	string name;
	MangleNameAndAppend(name, cls->GetUniqueName());
	name += "0_typeinfo";
	auto newv = new GlobalVariable(*module, GetTypeInfoType()->llvm_type,
		true, GlobalVariable::LinkOnceODRLinkage, nullptr,
		name, nullptr, GlobalValue::ThreadLocalMode::NotThreadLocal, 0, true);
	gen_context.rtti_map[cls] = newv;
	return newv;
}

template<typename Derived>
Derived* dyncast_resolve_anno(StatementAST* p)
{
	if (typeid(*p) == typeid(AnnotationStatementAST)) {
		return dyncast_resolve_anno<Derived>(static_cast<AnnotationStatementAST*>(p)->impl.get());
	}
	if (typeid(*p) == typeid(Derived)) {
		return static_cast<Derived*>(p);
	}
	return nullptr;
}

static const string& GetMangledSymbolPrefix()
{
	if (gen_context.mangled_symbol_prefix.empty())
		MangleNameAndAppend(gen_context.mangled_symbol_prefix, cu.symbol_prefix);
	return gen_context.mangled_symbol_prefix;
}

Value* GetObjOfMemberFunc(ExprAST* Callee)
{
	auto proto = Callee->resolved_type.proto_ast;
	Value* obj = nullptr;
	if (proto->cls)
	{
		auto pobj = dyncast_resolve_anno<MemberExprAST>(Callee);
		if (pobj)
			obj = pobj->llvm_obj;
		else if (auto piden = dyncast_resolve_anno<IdentifierExprAST>(Callee))
		{
			assert(dyncast_resolve_anno<MemberExprAST>(piden->impl.get()));
			obj = dyncast_resolve_anno<MemberExprAST>(piden->impl.get())->llvm_obj;
		}
		else if (auto pidx = dyncast_resolve_anno<IndexExprAST>(Callee))
		{
			if (isa<FunctionTemplateInstanceExprAST>(pidx->instance.get()))
			{
				auto ptr = static_cast<FunctionTemplateInstanceExprAST*> (pidx->instance.get());
				pobj = dyncast_resolve_anno<MemberExprAST>(ptr->expr.get());
				assert(pobj);
				obj = pobj->llvm_obj;
			}
			else
			{
				assert(isa<MemberExprAST>(pidx->instance.get()));
				pobj = static_cast<MemberExprAST*> (pidx->instance.get());
				obj = pobj->llvm_obj;
			}

		}
		else 
		{
			auto pfuncinst = dyncast_resolve_anno<FunctionTemplateInstanceExprAST>(Callee);
			assert(pfuncinst);
			pobj = dyncast_resolve_anno<MemberExprAST>(pfuncinst->expr.get());
			assert(pobj);
			obj = pobj->llvm_obj;
		}
		
	}
	return obj;
}

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
	DIFile *Unit = DBuilder->createFile(dinfo.cu->getFilename(),
		dinfo.cu->getDirectory());
	vector<llvm::Type*> types{llvm::Type::getInt32Ty(context),ArrayType::get(ty,0)};
	SmallVector<Metadata *, 8> dtypes{
		DBuilder->createMemberType(dinfo.cu,"size",Unit,1,32,32,0,DINode::DIFlags::FlagZero,dinfo.GetIntType()),
		DBuilder->createMemberType(dinfo.cu,"array",Unit,1,0,32,32,DINode::DIFlags::FlagZero,DBuilder->createArrayType(0, 0, dty,{}))
	}; //fix-me: what is the last parameter for???

	name +="_2_3";

	outdtype = DBuilder->createStructType(dinfo.cu, name, Unit, 0, 32, 64, DINode::DIFlags::FlagZero, nullptr, DBuilder->getOrCreateArray(dtypes));
	outdtype = DBuilder->createPointerType(outdtype, 64);
	return  StructType::create(context, types, name)->getPointerTo();

}

static void GenerateClosureTypes(llvm::Type* functy, DIType* funcdty, llvm::Type*& outbase, DIType*& outdtype, int lineno)
{
	outbase = StructType::create({ functy,builder.getInt8PtrTy() });
	auto size = module->getDataLayout().getTypeAllocSizeInBits(outbase);
	auto align = module->getDataLayout().getPrefTypeAlignment(outbase);

	DIFile *Unit = DBuilder->createFile(dinfo.cu->getFilename(),
		dinfo.cu->getDirectory());
	SmallVector<Metadata *, 8> dtypes = { 
		DBuilder->createMemberType(dinfo.cu,"pfunc",Unit,1,64,align*8,0,DINode::DIFlags::FlagZero, funcdty) ,
		DBuilder->createMemberType(dinfo.cu,"pclosure",Unit,1,64,align * 8,64,DINode::DIFlags::FlagZero,
		DBuilder->createBasicType("pointer",64,dwarf::DW_ATE_address))
	};
	outdtype = DBuilder->createStructType(dinfo.cu, "", Unit, lineno, size, align * 8, DINode::DIFlags::FlagZero, nullptr, DBuilder->getOrCreateArray(dtypes));

}

void GenerateType(const Birdee::ResolvedType& type, PDIType& dtype, llvm::Type* & base)
{
	if (type.index_level > 0)
	{
		llvm::Type* mybase;
		PDIType mydtype;
		Birdee::ResolvedType subtype = type;
		subtype.index_level--;

		auto node = helper.GetTypeNode(subtype);
		mybase = node.llvm_ty;
		mydtype = node.dty;

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
	case tok_null:
	case tok_pointer:
		base = llvm::Type::getInt8PtrTy(context);
		dtype = DBuilder->createBasicType("pointer", 64, dwarf::DW_ATE_address);
		break;
	case tok_func:
	{
		auto functy = type.proto_ast->GenerateFunctionType()->getPointerTo();
		auto funcdty = type.proto_ast->GenerateDebugType();
		if (!type.proto_ast->is_closure)
		{
			base = functy;
			dtype = funcdty;
		}
		else
		{
			GenerateClosureTypes(functy, funcdty, base, dtype, type.proto_ast->pos.line);
		}
		break;
	}
	case tok_void:
		base = llvm::Type::getVoidTy(context);
		dtype = DBuilder->createBasicType("void", 0, dwarf::DW_ATE_address);
		break;
	case tok_class:
		if (!type.class_ast->llvm_type)
		{
			string name;
			MangleNameAndAppend(name, type.class_ast->GetUniqueName());
			base = StructType::create(context, name);

			DIFile *Unit;
			if (type.class_ast->isTemplateInstance() && type.class_ast->template_source_class->template_param->mod) //if is templ instance and is imported
			{
				auto mod = type.class_ast->template_source_class->template_param->mod;
				Unit = DBuilder->createFile(mod->source_file, mod->source_dir);
			}		
			else
			{
				Unit = DBuilder->createFile(dinfo.cu->getFilename(), dinfo.cu->getDirectory());
			}
			dtype = DBuilder->createReplaceableCompositeType(llvm::dwarf::DW_TAG_structure_type, name, dinfo.cu ,Unit, type.class_ast->Pos.line);
			type.class_ast->llvm_dtype = dtype;
		}
		else
		{
			base = type.class_ast->llvm_type;
			dtype = type.class_ast->llvm_dtype;
		}
		if (!type.class_ast->is_struct)
		{
			base = base->getPointerTo();
			dtype= DBuilder->createPointerType(dtype, 64);
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
	auto type_n = helper.GetTypeNode(resolved_type);
	auto type = type_n.llvm_ty;
	DIType* ty = type_n.dty;
	string resolved_name;
	MangleNameAndAppend(resolved_name, package_name);
	resolved_name += "_0";
	MangleNameAndAppend(resolved_name, name);
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
	auto type_n = helper.GetTypeNode(resolved_type);
	auto type = type_n.llvm_ty;
	DIType* ty = type_n.dty;
	string var_name = GetMangledSymbolPrefix() + name;
	GlobalVariable* v = new GlobalVariable(*module, type,false,GlobalValue::CommonLinkage,
		Constant::getNullValue(type), var_name);
	DIGlobalVariableExpression* D = DBuilder->createGlobalVariableExpression(
			dinfo.cu, var_name, var_name, dinfo.cu->getFile(), Pos.line, ty,
			true);
	llvm_value = v;
	
	v->addDebugInfo(D); 
}

void Birdee::VariableSingleDefAST::PreGenerateForArgument(llvm::Value* init,int argno)
{
	if (!llvm_value)
	{
		auto type_n = helper.GetTypeNode(resolved_type);
		auto t = type_n.llvm_ty;
		DIType* ty = type_n.dty;
		assert(capture_import_type == CAPTURE_NONE);
		if (capture_export_type == CAPTURE_VAL)
		{
			assert(gen_context.cur_func && gen_context.cur_func->exported_capture_pointer);
			llvm_value = builder.CreateGEP(gen_context.cur_func->exported_capture_pointer,
				{ builder.getInt32(0),builder.getInt32(capture_export_idx + (gen_context.cur_func->capture_this ? 1 : 0)) }, name);
		}
		else
		{
			assert(capture_export_type == CAPTURE_NONE);
			llvm_value = builder.CreateAlloca(t, nullptr, name);
		}


		// Create a debug descriptor for the variable.
		DILocalVariable *D = DBuilder->createParameterVariable(
			dinfo.LexicalBlocks.back(), name, argno, dinfo.cu->getFile(), Pos.line, ty,
			true);

		DBuilder->insertDeclare(llvm_value, D, DBuilder->createExpression(),
			DebugLoc::get(Pos.line, Pos.pos, dinfo.LexicalBlocks.back()),
			builder.GetInsertBlock());
	}
	builder.CreateStore(init, llvm_value);
}

llvm::Value* Birdee::VariableSingleDefAST::Generate()
{
	dinfo.emitLocation(this);
	if (!llvm_value)
	{
		auto type_n = helper.GetTypeNode(resolved_type);
		auto t = type_n.llvm_ty;
		DIType* ty = type_n.dty;
		assert(capture_import_type == CAPTURE_NONE);
		if (capture_export_type == CAPTURE_VAL)
		{
			assert(gen_context.cur_func && gen_context.cur_func->exported_capture_pointer);
			llvm_value = builder.CreateGEP(gen_context.cur_func->exported_capture_pointer,
				{ builder.getInt32(0),builder.getInt32(capture_export_idx + (gen_context.cur_func->capture_this ? 1 : 0))}, name);
		}
		else
		{
			assert(capture_export_type == CAPTURE_NONE);
			llvm_value = builder.CreateAlloca(t, nullptr, name);
		}

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

DISubprogram * PrepareFunctionDebugInfo(Function* TheFunction,DISubroutineType* type,SourcePos pos, ImportedModule* mod)
{

	// Create a subprogram DIE for this function.
	DIFile *Unit = mod? DBuilder->createFile(mod->source_file,mod->source_dir) : dinfo.cu->getFile();
	unsigned LineNo = pos.line;
	unsigned ScopeLine = LineNo;
	DISubprogram *SP = DBuilder->createFunction(
		Unit, TheFunction->getName(), StringRef(), Unit, LineNo,
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

	//if is core lib, first generate type_info
	if (cu.is_corelib)
	{
		string str = "type_info";
		classmap.find(str)->second.get().PreGenerate();
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

	Function *F = Function::Create(FT, Function::ExternalLinkage, cu.expose_main ? "main" : GetMangledSymbolPrefix() + "_1main", module);
	BasicBlock *BB = BasicBlock::Create(context, "entry", F);
	builder.SetInsertPoint(BB);
	//SmallVector<Metadata *, 8> dargs{ DBuilder->createBasicType("void", 0, dwarf::DW_ATE_address) };
	DISubroutineType* functy = DBuilder->createSubroutineType(DBuilder->getOrCreateTypeArray({ DBuilder->createBasicType("void", 0, dwarf::DW_ATE_address) }));
	auto dbginfo = PrepareFunctionDebugInfo(F, functy, SourcePos(0,1, 1),nullptr);
	dinfo.LexicalBlocks.push_back(dbginfo);

	helper.cur_llvm_func = F;
	// Push the current scope.

	//check if I have initialized
	GlobalVariable* check_init = new GlobalVariable(*module, builder.getInt1Ty(), false, GlobalValue::PrivateLinkage,
		builder.getInt1(false), GetMangledSymbolPrefix() + "_1init");

	BasicBlock *BT = BasicBlock::Create(context, "init", F);
	BasicBlock *BF = BasicBlock::Create(context, "no_init", F);
	builder.CreateCondBr(builder.CreateLoad(check_init), BT, BF);

	builder.SetInsertPoint(BT);
	builder.CreateRetVoid();

	builder.SetInsertPoint(BF);
	builder.CreateStore(builder.getInt1(true), check_init);

	for (auto& name : cu.imported_module_names)
	{
		string func_name;
		MangleNameAndAppend(func_name, name);
		func_name += "_0_1main";
		Function *OtherMain = Function::Create(FT, Function::ExternalLinkage, func_name, module);
		builder.SetCurrentDebugLocation(DebugLoc::get(0, 0, F->getSubprogram()));
		builder.CreateCall(OtherMain);
	}
	
	if (toplevel.size() > 0)
	{
		for (auto& stmt : toplevel)
		{
			stmt->Generate();
		}
		if (!dyncast_resolve_anno<ReturnAST>(toplevel.back().get()))
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
}

llvm::FunctionType * Birdee::PrototypeAST::GenerateFunctionType()
{
	std::vector<llvm::Type*> args;
	if (cls) {
		args.push_back(cls->llvm_type->getPointerTo());
	}
	if (is_closure)
	{
		args.push_back(builder.getInt8PtrTy());
		assert(cls == nullptr);
	}
	for (auto& arg : resolved_args)
	{
		llvm::Type* argty = helper.GetType(arg->resolved_type);
		args.push_back(argty);
	}
	return FunctionType::get(helper.GetType(resolved_type),args,false);
}

DIType * Birdee::PrototypeAST::GenerateDebugType()
{
	SmallVector<Metadata *, 8> dargs;
	// Add the result type.
	auto type_n = helper.GetTypeNode(resolved_type);
	dargs.push_back(type_n.dty);
	
	if (cls) {
		dargs.push_back(DBuilder->createPointerType(cls->llvm_dtype,64));
	}
	if (is_closure)
	{
		dargs.push_back(DBuilder->createBasicType("pointer", 64, dwarf::DW_ATE_address));
	}
	for (auto& arg : resolved_args)
	{
		DIType* darg = helper.GetTypeNode(arg->resolved_type).dty;
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
	// parent class should call PreGenerate() before
	if (parent_type && !parent_class->llvm_type)
		parent_class->PreGenerate();

	vector<llvm::Type*> types;
	SmallVector<Metadata *, 8> dtypes;

	int field_offset = 0;
	LLVMHelper::TypePair type_info_llvm_pair;
	if (needs_rtti && !parent_class) //only generate rtti when it is not a sub-class
	{
		field_offset++;
		ClassAST* type_info_ty = nullptr;
		type_info_ty = GetTypeInfoType();
		assert(type_info_ty->llvm_dtype);
		assert(type_info_ty->llvm_type);
		type_info_llvm_pair = LLVMHelper::TypePair{ type_info_ty->llvm_type->getPointerTo(),
			DBuilder->createPointerType(type_info_ty->llvm_dtype, 64) };
	}
	if (parent_class)
	{
		field_offset++;
	}

	DIFile *Unit;
	if (isTemplateInstance() && template_source_class->template_param->mod) //if is templ instance and is imported
	{
		auto mod = template_source_class->template_param->mod;
		Unit = DBuilder->createFile(mod->source_file, mod->source_dir);
	}
	else
		Unit = DBuilder->createFile(dinfo.cu->getFilename(), dinfo.cu->getDirectory());
	vector<LLVMHelper::TypePair*> ty_nodes;
	ty_nodes.reserve(fields.size() + field_offset);
	if (needs_rtti && !parent_class)
	{
		types.push_back(type_info_llvm_pair.llvm_ty);
		ty_nodes.push_back(&type_info_llvm_pair);
	}
	if (parent_class)
	{
		auto& node2 = helper.GetTypeNode(ResolvedType(parent_class));
		types.push_back(node2.llvm_ty->getPointerElementType());
		ty_nodes.push_back(&node2);
	}
	for (auto& field : fields)
	{
		auto& node2 = helper.GetTypeNode(field.decl->resolved_type);
		types.push_back(node2.llvm_ty);
		ty_nodes.push_back(&node2);
	}
	auto& node = helper.GetTypeNode(ResolvedType(this));
	auto ty =  node.llvm_ty;
	if (!is_struct)
		llvm_type = (llvm::StructType*)ty->getPointerElementType();
	else
		llvm_type = (llvm::StructType*)ty;
	llvm_type->setBody(types);

	auto size = module->getDataLayout().getTypeAllocSizeInBits(llvm_type);
	auto align = module->getDataLayout().getPrefTypeAlignment(llvm_type);

	{
		auto& Pos = this->Pos;
		auto& llvm_type = this->llvm_type;
		auto push_dtype = [&dtypes, &ty_nodes, &Unit, &Pos, align, &llvm_type](const std::string& name, int idx)
		{
			auto fsize = module->getDataLayout().getTypeAllocSizeInBits(ty_nodes[idx]->llvm_ty);
			auto memb = DBuilder->createMemberType(dinfo.cu, name, Unit, Pos.line, fsize, align,
				module->getDataLayout().getStructLayout((StructType*)llvm_type)->getElementOffsetInBits(idx), DINode::DIFlags::FlagZero,
				ty_nodes[idx]->dty);
			dtypes.push_back(memb);
		};

		if (needs_rtti && !parent_class)
			push_dtype("__rtti_ptr__", 0);
		if (parent_type)
			push_dtype("__class_parent__", field_offset - 1);
		int _idx = field_offset;
		for (auto& field : fields)
		{
			push_dtype(field.decl->name, _idx);
			_idx++;
		}
	}

	ty_nodes.clear();

	auto new_dty=DBuilder->createStructType(dinfo.cu, llvm_type->getName(), Unit, Pos.line, size, align*8, DINode::DIFlags::FlagZero, nullptr, DBuilder->getOrCreateArray(dtypes));
	if(llvm_dtype)
		llvm_dtype->replaceAllUsesWith(new_dty);
	llvm_dtype = new_dty;
	node.dty = is_struct ? llvm_dtype: DBuilder->createPointerType(llvm_dtype, 64);
	
	if (needs_rtti && !parent_class)
	{
		GetOrCreateTypeInfoGlobal(this);
	}
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
		return helper.typemap[resolved_type].dty;
	if (intrinsic_function) //if it is an intrinsic function, generate a null ptr
	{
		llvm_func = Constant::getNullValue(Proto->GenerateFunctionType()->getPointerTo());
		DIType* ret = Proto->GenerateDebugType();
		helper.typemap[resolved_type].dty = ret;
		helper.typemap[resolved_type].llvm_ty = llvm_func->getType();
		return ret;
	}
	string prefix;
	if (Proto->cls)
	{
		MangleNameAndAppend(prefix, Proto->cls->GetUniqueName());
		prefix += "_0";
	}
	else if (Proto->prefix_idx == -1)
		prefix = GetMangledSymbolPrefix();
	else
	{
		MangleNameAndAppend(prefix, cu.imported_module_names[Proto->prefix_idx]);
		prefix += "_0";
	}
	if (parent)
	{
		auto f = parent;
		vector<string*> names;
		while (f)
		{
			names.push_back(&f->Proto->Name);
			f = f->parent;
		}
		for (auto itr = names.rbegin(); itr != names.rend(); itr++)
		{
			//prefix += **itr;
			MangleNameAndAppend(prefix, **itr);
			prefix += "_0";
		}

	}
	GlobalValue::LinkageTypes linkage;
	if (Proto->cls && Proto->cls->isTemplateInstance() || isTemplateInstance)
		linkage = Function::LinkOnceODRLinkage;
	else
		linkage = Function::ExternalLinkage;
	MangleNameAndAppend(prefix, Proto->GetName());
	auto ftype=Proto->GenerateFunctionType();
	auto myfunc = Function::Create(ftype, linkage, prefix, module);
	llvm_func = myfunc;
	if (strHasEnding(cu.targetpath,".obj") && (Proto->cls && Proto->cls->isTemplateInstance() || isTemplateInstance))
	{
		myfunc->setComdat(module->getOrInsertComdat(llvm_func->getName()));
		myfunc->setDSOLocal(true);
	}
	DIType* ret = Proto->GenerateDebugType();
	auto itr = helper.typemap.find(resolved_type);
	if(itr==helper.typemap.end())
	{
		LLVMHelper::TypePair tynode;
		if(resolved_type.proto_ast->is_closure)
		{
			GenerateClosureTypes(ftype->getPointerTo(),ret,tynode.llvm_ty, tynode.dty, Pos.line);
		}
		else
		{
			tynode.dty = ret;
			tynode.llvm_ty = ftype->getPointerTo();
		}
		helper.typemap.insert(std::make_pair(resolved_type,tynode));
	}

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

Value* DoGenerateCall(Value* func,const vector<Value*>& args)
{
	if (gen_context.landingpad) //if we are in try
	{
		BasicBlock *BB = BasicBlock::Create(context, "normal", helper.cur_llvm_func);
		auto ret = builder.CreateInvoke(func, BB, gen_context.landingpad, args);
		builder.SetInsertPoint(BB);
		return ret;
	}
	else
		return builder.CreateCall(func, args);
}

Value* GenerateCall(Value* func, PrototypeAST* proto, Value* obj, const vector<unique_ptr<ExprAST>>& Args,SourcePos pos)
{
	vector<Value*> args;
	if (obj)
	{
		args.push_back(obj);
	}
	if (proto->is_closure)
	{
		args.push_back(builder.CreateExtractValue(func, 1)); //push the closure capture
		func = builder.CreateExtractValue(func, 0);
	}
	for (auto& vargs : Args)
	{
		args.push_back(vargs->Generate());
	}

	builder.SetCurrentDebugLocation(
		DebugLoc::get(pos.line, pos.pos, helper.cur_llvm_func->getSubprogram()));
	return DoGenerateCall(func, args);
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
	assert(!resolved_type.class_ast->is_struct);
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

	if (resolved_type.class_ast->needs_rtti)
	{
		ClassAST* curcls = resolved_type.class_ast;
		std::vector<Value*> gep = { builder.getInt32(0),builder.getInt32(0) };
		while (curcls->parent_class)
		{
			gep.push_back(builder.getInt32(0));
			curcls = curcls->parent_class;
		}
		builder.CreateStore(GetOrCreateTypeInfoGlobal(resolved_type.class_ast),
			builder.CreateGEP(ret, gep));
	}

	if(func)
		GenerateCall(func->llvm_func, func->Proto.get(), ret, args, this->Pos);
	return ret;
}

bool Birdee::ASTBasicBlock::Generate()
{
	for (auto& stmt : body)
	{
		stmt->Generate();
	}
	if (!builder.GetInsertBlock()->getInstList().empty() && builder.GetInsertBlock()->getInstList().back().isTerminator())
	{
		return true;
	}
	return false;
}

llvm::Value * Birdee::UpcastExprAST::Generate()
{
	return nullptr;
}

llvm::Value * Birdee::ThisExprAST::Generate()
{
	if (resolved_type.class_ast->is_struct)
		return builder.CreateLoad(GeneratePtr());
	else
		return GeneratePtr();
}

llvm::Value * Birdee::ThisExprAST::GeneratePtr()
{
	dinfo.emitLocation(this);
	if (gen_context.cur_func && gen_context.cur_func->parent)//(gen_context.cur_func && gen_context.cur_func->parent && gen_context.cur_func->parent->capture_this)
	{
		//if we are in a function & the function is a lambda & the parent function has captured this
		assert(gen_context.cur_func->Proto->is_closure);
		return gen_context.cur_func->captured_parent_this;
	}
	else
	{
		assert(gen_context.cur_func->Proto->cls);
		return helper.cur_llvm_func->args().begin();
	}
}

llvm::Value * Birdee::SuperExprAST::Generate()
{
	dinfo.emitLocation(this);
	if (gen_context.cur_func && gen_context.cur_func->parent)
	{
		// if we are in a function & the function is a lambda & the parent function has captured super
	 	assert(gen_context.cur_func->Proto->is_closure);
	 	return gen_context.cur_func->captured_parent_super;
	}
	else
	{
		assert(gen_context.cur_func->Proto->cls);
		auto this_llvm_obj = helper.cur_llvm_func->args().begin();
		return builder.CreateGEP(this_llvm_obj, { builder.getInt32(0),builder.getInt32(0) });
	}
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

	if (intrinsic_function)
	{
		return nullptr;
	}
	ImportedModule* mod = nullptr;
	if (isTemplateInstance && template_source_func->template_param->mod)
		mod = template_source_func->template_param->mod;
	else if (Proto->cls && Proto->cls->isTemplateInstance()
		&& Proto->cls->template_source_class->template_param->mod)
		mod = Proto->cls->template_source_class->template_param->mod;

	assert(llvm::isa<llvm::Function>(*llvm_func));
	Function* myfunc = static_cast<Function*>(llvm_func);
	DISubprogram* dbginfo = PrepareFunctionDebugInfo(myfunc, functy, Pos, mod);

	if (!isDeclare)
	{
		assert(myfunc->getBasicBlockList().empty());
		auto curfunc_backup = gen_context.cur_func;
		auto landingpad_backup = gen_context.landingpad;
		gen_context.cur_func = this;
		dinfo.LexicalBlocks.push_back(dbginfo);
		auto IP = builder.saveIP();
		auto func_backup = helper.cur_llvm_func;
		helper.cur_llvm_func = myfunc;
		BasicBlock *BB = BasicBlock::Create(context, "entry", myfunc);
		builder.SetInsertPoint(BB);

		dinfo.emitLocation(this);
		auto itr = myfunc->args().begin();
		int param_offset = 0;
		if (Proto->cls)
		{
			DILocalVariable *D = DBuilder->createParameterVariable(
				dinfo.LexicalBlocks.back(), "this", 1, dinfo.cu->getFile(), Pos.line, helper.GetTypeNode(ResolvedType(Proto->cls)).dty,
				true);

			DBuilder->insertDeclare(itr, D, DBuilder->createExpression(),
				DebugLoc::get(Pos.line, Pos.pos, dinfo.LexicalBlocks.back()),
				builder.GetInsertBlock());
			param_offset = 1;
			itr++;
		}
		if (Proto->is_closure)
		{//if is closure, generate the imported captured var
			imported_capture_pointer = builder.CreatePointerCast(itr,parent->exported_capture_type->getPointerTo());
			size_t idx = (parent->capture_this || parent->capture_super) ? 1 : 0;
			if (captured_parent_this) //if "this" is referenced in the function
			{//get the real "this" in the imported captured context
				captured_parent_this = builder.CreateLoad(builder.CreateGEP(imported_capture_pointer, { builder.getInt32(0),builder.getInt32(0) }), "this");
			}
			if (captured_parent_super)
			{
				ClassAST * cls = nullptr;
				FunctionAST * func = this;
				// find the original class it belongs to
				do {
					if (func->Proto->cls)
					{
						cls = func->Proto->cls;
						break;
					}
					func = func->parent;
				} while (func);
				assert(cls);
				auto this_llvm_value = builder.CreateLoad(builder.CreateGEP(imported_capture_pointer, { builder.getInt32(0),builder.getInt32(0) }), "this");
				captured_parent_super = builder.CreateGEP(this_llvm_value, { builder.getInt32(0),builder.getInt32(0) });
			}
			for (auto& v : imported_captured_var)
			{
				auto var = v.second.get();
				DIType* ty = helper.GetTypeNode(var->resolved_type).dty;
				assert(v.second->capture_import_type != VariableSingleDefAST::CAPTURE_NONE);
				int impidx = v.second->capture_import_idx;
				if(v.second->capture_import_type==VariableSingleDefAST::CAPTURE_VAL)
					var->llvm_value = builder.CreateGEP(imported_capture_pointer, { builder.getInt32(0),builder.getInt32(impidx) }, var->name);
				else
					var->llvm_value = builder.CreateLoad(builder.CreateGEP(imported_capture_pointer, { builder.getInt32(0),builder.getInt32(impidx) }), var->name);
				// Create a debug descriptor for the variable.
				DILocalVariable *D = DBuilder->createAutoVariable(dinfo.LexicalBlocks.back(), var->name, dinfo.cu->getFile(), var->Pos.line, ty,
					true);

				DBuilder->insertDeclare(var->llvm_value, D, DBuilder->createExpression(),
					DebugLoc::get(var->Pos.line, var->Pos.pos, dinfo.LexicalBlocks.back()),
					builder.GetInsertBlock());
				idx++;
			}
			itr++;
		}
		if (captured_var.size() || capture_this || capture_super)
		{ //if my own variables are captured by children functions
			vector<llvm::Type*> captype;
			if (capture_this || capture_super)
			{
				assert(Proto->cls);
				captype.push_back(Proto->cls->llvm_type->getPointerTo());
			}
			for (auto v : captured_var)
			{
				auto type = helper.GetType(v->resolved_type);
				if (v->capture_export_type == VariableSingleDefAST::CAPTURE_REF)
					type = type->getPointerTo();
				captype.push_back(type);
			}
			exported_capture_type = StructType::create(context, captype, (llvm_func->getName() + "_0_1context").str());
			if (capture_on_stack)
			{
				exported_capture_pointer = builder.CreateAlloca(exported_capture_type, nullptr, "_1export_capture_pointer");
			}
			else
			{
				size_t sz = module->getDataLayout().getTypeAllocSize(exported_capture_type);
				exported_capture_pointer = builder.CreatePointerCast(
					builder.CreateCall(GetMallocObj(), { builder.getInt32(sz),Constant::getNullValue(builder.getInt8PtrTy()) }),
					exported_capture_type->getPointerTo(), ".export_capture_pointer");
			}
			if (capture_this || capture_super)
			{
				auto target = builder.CreateGEP(exported_capture_pointer, { builder.getInt32(0),builder.getInt32(0) });
				if(Proto->is_closure)
					builder.CreateStore(captured_parent_this, target);
				else
					builder.CreateStore(myfunc->args().begin(), target);
			}
			for (auto v : captured_var)
			{
				if (v->capture_export_type == VariableSingleDefAST::CAPTURE_REF)
				{
					//export the variables that are captured by reference from imported context
					assert(v->capture_import_type != VariableSingleDefAST::CAPTURE_NONE && v->llvm_value);
					builder.CreateStore(v->llvm_value,
						builder.CreateGEP(exported_capture_pointer, { builder.getInt32(0),builder.getInt32((capture_this ? 1 : 0) + v->capture_export_idx) }));
				}
			}
			//the exported variables will be generated in variable definition statements
		}
		
		for (int i = 0;itr != myfunc->args().end(); itr++,i++)
		{
			Proto->resolved_args[i]->PreGenerateForArgument(itr, i + 1 + param_offset);
		}


		bool hasret = Body.Generate();
		if (!hasret)
		{
			if (resolved_type.proto_ast->resolved_type.type == tok_void)
				builder.CreateRetVoid();
			else
				builder.CreateRet(Constant::getNullValue(myfunc->getReturnType()));
		}
		dinfo.LexicalBlocks.pop_back();
		builder.restoreIP(IP);
		helper.cur_llvm_func = func_backup;
		gen_context.cur_func = curfunc_backup;
		gen_context.landingpad = landingpad_backup;
	}
	else if (!isImported) //if is declaration and is a c-style extern function
	{
		if(link_name.empty())
			llvm_func->setName(Proto->GetName());
		else
			llvm_func->setName(link_name);
	}
	if (Proto->is_closure)
	{
		myfunc->setLinkage(llvm::GlobalValue::LinkageTypes::InternalLinkage);
		dinfo.emitLocation(this);
		auto type = helper.GetType(resolved_type);
		assert(llvm::isa<StructType>(*type));
		auto ptr = builder.CreateInsertValue(UndefValue::get(type), llvm_func, 0);
		return builder.CreateInsertValue(ptr, 
			builder.CreatePointerCast(parent->exported_capture_pointer,builder.getInt8PtrTy()), 1);
	}
	else
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
	if (def->isTemplate())
		return nullptr;
	if (!def->llvm_func)
		def->Generate();
	dinfo.emitLocation(this);
	return def->llvm_func;
}


llvm::Value * Birdee::LoopControlAST::Generate()
{
	if (!helper.cur_loop.isNotNull())
		throw CompileError(Pos, "continue or break cannot be used outside of a loop");
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
	gen_context.cls_string = GetStringClass()->llvm_type;
	return gen_context.cls_string;
}

llvm::Value * Birdee::BoolLiteralExprAST::Generate()
{
	return builder.getInt1(v);
}


static llvm::GlobalVariable* GenerateStr(const StringRefOrHolder& Val)
{
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

	Constant * str = ConstantDataArray::getString(context, Val.get());
	vector<llvm::Type*> types{ llvm::Type::getInt32Ty(context),ArrayType::get(builder.getInt8Ty(),Val.get().length() + 1) };
	auto cur_array_ty = StructType::create(context, types);

	Constant * strarr = llvm::ConstantStruct::get(cur_array_ty, {
		ConstantInt::get(llvm::Type::getInt32Ty(context),APInt(32,Val.get().length() + 1,true)),
		str
		});

	GlobalVariable* vstr = new GlobalVariable(*module, strarr->getType(), true, GlobalValue::PrivateLinkage, nullptr);
	vstr->setInitializer(strarr);

	std::vector<Constant*> const_ptr_5_indices;
	ConstantInt* const_int64_6 = ConstantInt::get(context, APInt(64, 0));
	const_ptr_5_indices.push_back(const_int64_6);
	Constant* const_ptr_5 = ConstantExpr::getGetElementPtr(nullptr, vstr, const_ptr_5_indices);
	const_ptr_5 = ConstantExpr::getPointerCast(const_ptr_5, gen_context.byte_arr_ty);

	Constant * obj = llvm::ConstantStruct::get(GetStringType(), {
		const_ptr_5,
		ConstantInt::get(llvm::Type::getInt32Ty(context),APInt(32,Val.get().length(),true))
		});
	GlobalVariable* vobj = new GlobalVariable(*module, GetStringType(), true, GlobalValue::PrivateLinkage, nullptr);
	vobj->setInitializer(obj);
	gen_context.stringpool[Val] = vobj;
	return vobj;
}
llvm::Value * Birdee::StringLiteralAST::Generate()
{
	dinfo.emitLocation(this);
	return GenerateStr(StringRefOrHolder(&Val)); //generate a string and put the string in a pool with string reference
}

llvm::Value * Birdee::LocalVarExprAST::Generate()
{
	dinfo.emitLocation(this);
	return builder.CreateLoad(def->llvm_value);
}

// llvm::Value * Birdee::AddressOfExprAST::Generate()
// {
// 	dinfo.emitLocation(this);
// 	if(!is_address_of)
// 		return builder.CreateBitOrPointerCast(expr->Generate(), llvm::Type::getInt8PtrTy(context));
// 	auto ret=expr->GetLValue(false);
// 	assert(ret);
// 	return builder.CreateBitOrPointerCast(ret, llvm::Type::getInt8PtrTy(context));
// }

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
	if (Val)
	{
		Value* ret = Val->Generate();
		return builder.CreateRet(ret);
	}
	else
	{
		return builder.CreateRet(nullptr);
	}
}

llvm::Value * Birdee::NullExprAST::Generate()
{
	return Constant::getNullValue(helper.GetType(resolved_type));
}

llvm::Value * Birdee::IndexExprAST::GetLValue(bool checkHas)
{
	unique_ptr<ExprAST>*dummy;
	if (checkHas)//if expr is moved, it is either a template instance or a overloaded call to __getitem__
		return (Expr==nullptr || isTemplateInstance(dummy)) ? nullptr: (llvm::Value *)1 ;
	assert(Expr);
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
	if (func_callee && func_callee->intrinsic_function) // if we can find the FunctionAST* from callee & it's an intrinsic function
	{
		Value* obj = GetObjOfMemberFunc(Callee.get());
		return func_callee->intrinsic_function->Generate(func_callee, obj, Args);
	}
	auto func=Callee->Generate();
	assert(Callee->resolved_type.type == tok_func);
	auto proto = Callee->resolved_type.proto_ast;
	Value* obj = GetObjOfMemberFunc(Callee.get());
	return GenerateCall(func, proto, obj, Args,this->Pos);
}
namespace Birdee
{
	extern ClassAST* GetArrayClass();
}

static Value* GenerateMemberParentGEP(Value* casade_llvm_obj, ClassAST* &curcls,  int casade_parents)
{
	if (casade_parents)
	{
		std::vector<Value*> gep = { builder.getInt32(0) };
		for (int i = 0; i < casade_parents; i++)
		{
			gep.push_back(builder.getInt32(0));
			curcls = curcls->parent_class;
		}
		return builder.CreateGEP(casade_llvm_obj, gep);
	}
	else
	{
		return casade_llvm_obj;
	}
}

llvm::Value * Birdee::MemberExprAST::Generate()
{
	dinfo.emitLocation(this);
	int field_offset = 0;	
	if (Obj)
	{
		if (Obj->resolved_type.type == tok_class && Obj->resolved_type.class_ast->is_struct)
		{
			llvm_obj = Obj->GetLValue(false);
			if (!llvm_obj)
			{
				if(auto thisexpr = dyncast_resolve_anno<ThisExprAST>(Obj.get()))
					llvm_obj = thisexpr->GeneratePtr();
			}
		}
		else if (Obj->resolved_type.type == tok_class && !Obj->resolved_type.class_ast->is_struct)
		{
			auto casade_llvm_obj = Obj->Generate();
			ClassAST* curcls = Obj->resolved_type.class_ast;
			llvm_obj = GenerateMemberParentGEP(casade_llvm_obj, curcls, casade_parents);
			//if current class is a subclass, add 1 to offset,
			//because field 0 is always the embeded parent class object
			if (curcls->parent_type)
				field_offset++;
			else if(curcls->needs_rtti) //if current class has no super class, then check if there is an embeded rtti pointer in the fields
				field_offset++;
		}
		else
		{
			llvm_obj = Obj->Generate();
			assert(llvm_obj);
		}

	}
	dinfo.emitLocation(this);
	if (kind == member_field)
	{
		if (llvm_obj)//if we have a pointer to the object
		{
			return builder.CreateLoad(builder.CreateGEP(llvm_obj, { builder.getInt32(0),builder.getInt32(field->index + field_offset) }));
		} else //else, we only have a RValue of struct
			return builder.CreateExtractValue(Obj->Generate(), field->index + field_offset);
	}
	else if (kind == member_function)
	{
		if (!gen_context.array_cls)
			gen_context.array_cls = GetArrayClass();
		if (Obj->resolved_type.index_level > 0)
			llvm_obj = builder.CreatePointerCast(llvm_obj, gen_context.array_cls->llvm_type->getPointerTo());
		if (!llvm_obj) //if we only have a RValue of struct
		{
			llvm_obj = builder.CreateAlloca(Obj->resolved_type.class_ast->llvm_type); //alloca temp memory for the value
			builder.CreateStore(Obj->Generate(), llvm_obj); //store the obj to the alloca
		}
		return func->decl->llvm_func;
	}
	else if (kind == member_imported_dim)
	{
		return builder.CreateLoad(import_dim->llvm_value);
	}
	else if (kind == member_imported_function)
	{
		//function template instance will be "imported function" too.
		if (Obj && !llvm_obj) //if there is a object reference and we only have a RValue of struct
		{
			llvm_obj = builder.CreateAlloca(Obj->resolved_type.class_ast->llvm_type); //alloca temp memory for the value
			builder.CreateStore(Obj->Generate(), llvm_obj); //store the obj to the alloca
		}

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

	if (needs_rtti) //the class is instantiated in the current module, generate type_info
	{
		auto tyinfo = GetOrCreateTypeInfoGlobal(this);
		tyinfo->setComdat(module->getOrInsertComdat(tyinfo->getName()));
		tyinfo->setExternallyInitialized(false);

		GlobalVariable* vstr = GenerateStr(StringRefOrHolder(GetUniqueName()));
		Constant* const_ptr_5 = ConstantExpr::getGetElementPtr(nullptr, vstr, { builder.getInt32(0) });

		auto val = ConstantStruct::get(GetTypeInfoType()->llvm_type, {
				const_ptr_5
			});
		tyinfo->setInitializer(val);
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
		if (Obj)
		{
			assert(Obj->resolved_type.isResolved());
			//if obj is a struct, whether its members are LValue depends on whether obj is a lvalue 
			if (Obj->resolved_type.type == tok_class && Obj->resolved_type.class_ast->is_struct)
			{
				if(Obj->GetLValue(true))
					return (llvm::Value *)1;
				else
				{
					//this expr is a special case, itself is a pointer. So its members are LValues
					if (dyncast_resolve_anno<ThisExprAST>(Obj.get()))
					{
						return (llvm::Value *)1;
					}
					//if obj is not LValue, the memberexpr is not LValue
					return nullptr;
				}
			}
		}
		return (llvm::Value *)1;
	}
	dinfo.emitLocation(this);
	int offset = 0;
	if (Obj->resolved_type.type == tok_class && Obj->resolved_type.class_ast->is_struct)
	{
			llvm_obj = Obj->GetLValue(false);
			if (!llvm_obj)
			{
				auto thisexpr = dyncast_resolve_anno<ThisExprAST>(Obj.get());
				assert(thisexpr);
				llvm_obj = thisexpr->GeneratePtr();
			}
	}
	else
		llvm_obj = Obj->Generate();

	if (kind == member_field)
	{
		assert(Obj->resolved_type.type == tok_class);
		auto curcls = Obj->resolved_type.class_ast;
		auto casade_llvm_obj = GenerateMemberParentGEP(llvm_obj, curcls, casade_parents);
		if (curcls->parent_type)
			offset++;
		else if (curcls->needs_rtti) //if current class has no super class, then check if there is an embeded rtti pointer in the fields
			offset++;
		return builder.CreateGEP(casade_llvm_obj, { builder.getInt32(0),builder.getInt32(field->index + offset) });
	}
		
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
	case tok_equal:
	case tok_cmp_equal:
		return builder.CreateICmpEQ(lv, rvexpr->Generate());
	case tok_ne:
	case tok_cmp_ne:
		return builder.CreateICmpNE(lv, rvexpr->Generate());
	default:
		assert(0 && "Operator not supported");
	}
	return nullptr;
}

llvm::Value * Birdee::BinaryExprAST::Generate()
{
	dinfo.emitLocation(this);
	if (func)
	{
		//if it is an overloaded operator, the call expr is in LHS
		return LHS->Generate();
	}
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
	}
	if (isLogicToken(Op) || (LHS->resolved_type.type==tok_boolean && LHS->resolved_type.index_level==0) ) //boolean
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

llvm::Value * Birdee::UnaryExprAST::Generate()
{
	dinfo.emitLocation(this);

	if (Op == tok_not)
	{
		if (func)
		{
			//if it is an overloaded operator, the call expr is in LHS
			return arg->Generate();
		}
		return builder.CreateNot(arg->Generate());
	}
	else if (Op == tok_address_of)
	{
		auto ret = arg->GetLValue(false);
		assert(ret);
		return builder.CreateBitOrPointerCast(ret, llvm::Type::getInt8PtrTy(context));
	}
	else if (Op == tok_pointer_of)
	{
		return builder.CreateBitOrPointerCast(arg->Generate(), llvm::Type::getInt8PtrTy(context));
	}
	else if (Op == tok_typeof)
	{
		auto val = arg->Generate();
		assert(arg->resolved_type.type == tok_class);
		auto curcls = arg->resolved_type.class_ast;

		std::vector<Value*> gep = { builder.getInt32(0),builder.getInt32(0) };
		while (curcls->parent_class)
		{
			gep.push_back(builder.getInt32(0)); //get the parent pointer
			curcls = curcls->parent_class;
		}
		return builder.CreateLoad(builder.CreateGEP(val, gep));
	}

	return nullptr;
}

llvm::Value* Birdee::AnnotationStatementAST::GetLValueNoCheckExpr(bool checkHas)
{
	assert(is_expr);
	return static_cast<ExprAST*>(impl.get())->GetLValue(checkHas);
}

Value * Birdee::AnnotationStatementAST::Generate()
{
	return impl->Generate();
}

Value * Birdee::FunctionToClosureAST::Generate()
{
	assert(resolved_type.proto_ast->cls == nullptr);
	Value* v = func->Generate();
	Function* outfunc;
	Value* outfunc_value;
	Value* capture;
	auto type = helper.GetType(resolved_type);
	assert(llvm::isa<StructType>(*type));
	StructType* stype = (StructType*)type;

	dinfo.emitLocation(this);
	if (func->resolved_type.proto_ast->cls) //if is binding for member function
	{
		outfunc_value = builder.CreatePointerCast(v,stype->getElementType(0));
		capture = builder.CreatePointerCast(GetObjOfMemberFunc(func.get()),builder.getInt8PtrTy());
	}
	else
	{
		if (llvm::isa<Function>(*v))
		{
			Function* target = (Function*)v;
			capture = Constant::getNullValue(builder.getInt8PtrTy());
			auto funcitr = gen_context.function_wrapper_cache.find(target);
			if (funcitr != gen_context.function_wrapper_cache.end())
			{
				outfunc_value = funcitr->second;
			}
			else
			{
				//if the value is a function definition, we can better optimize by creating a
				//warpper function specially for the function
				outfunc = Function::Create(proto->GenerateFunctionType(), GlobalValue::InternalLinkage, target->getName() + "_0_1wrapper", module);
				outfunc_value = outfunc;
				auto dbginfo = PrepareFunctionDebugInfo(outfunc, static_cast<DISubroutineType*>(proto->GenerateDebugType()), func->Pos, nullptr);

				dinfo.LexicalBlocks.push_back(dbginfo);
				auto IP = builder.saveIP();
				auto func_backup = helper.cur_llvm_func;
				helper.cur_llvm_func = outfunc;
				BasicBlock *BB = BasicBlock::Create(context, "entry", outfunc);
				builder.SetInsertPoint(BB);

				dinfo.emitLocation(this);

				auto itr = outfunc->args().begin();
				vector<Value*> args;
				for (/*ignore the first capture pointer*/ itr++; itr != outfunc->args().end(); itr++)
				{
					args.push_back(itr);
				}
				Value* ret = builder.CreateCall(target, args);
				if (resolved_type.proto_ast->resolved_type.type == tok_void)
					builder.CreateRetVoid();
				else
					builder.CreateRet(ret);
				dinfo.LexicalBlocks.pop_back();
				builder.restoreIP(IP);
				helper.cur_llvm_func = func_backup;
				gen_context.function_wrapper_cache[target] = outfunc;
			}
		}
		else
		{
			capture = builder.CreatePointerCast(v, builder.getInt8PtrTy());
			auto funcitr = gen_context.function_ptr_wrapper_cache.find(*proto);
			if (funcitr != gen_context.function_ptr_wrapper_cache.end())
			{
				outfunc_value = funcitr->second;
			}
			else
			{
				auto functype = proto->GenerateFunctionType();
				outfunc = Function::Create(functype, GlobalValue::InternalLinkage, "func_ptr_wrapper", module);
				outfunc_value = outfunc;
				auto dbginfo = PrepareFunctionDebugInfo(outfunc, static_cast<DISubroutineType*>(proto->GenerateDebugType()), func->Pos, nullptr);

				dinfo.LexicalBlocks.push_back(dbginfo);
				auto IP = builder.saveIP();
				auto func_backup = helper.cur_llvm_func;
				helper.cur_llvm_func = outfunc;
				BasicBlock *BB = BasicBlock::Create(context, "entry", outfunc);
				builder.SetInsertPoint(BB);

				dinfo.emitLocation(this);
				auto itr = outfunc->args().begin();
				Value* funcptr = builder.CreatePointerCast(itr, v->getType());
				vector<Value*> args;
				for (/*ignore the first capture pointer*/ itr++; itr != outfunc->args().end(); itr++)
				{
					args.push_back(itr);
				}
				Value* ret = builder.CreateCall(funcptr, args);
				if (resolved_type.proto_ast->resolved_type.type == tok_void)
					builder.CreateRetVoid();
				else
					builder.CreateRet(ret);
				dinfo.LexicalBlocks.pop_back();
				builder.restoreIP(IP);
				helper.cur_llvm_func = func_backup;
				gen_context.function_ptr_wrapper_cache[*proto] = outfunc;
			}
		}
	}

	dinfo.emitLocation(this);
	auto ptr = builder.CreateInsertValue(UndefValue::get(type), outfunc_value, 0);
	ptr = builder.CreateInsertValue(ptr, capture, 1);
	return ptr;
}

// llvm::Value * Birdee::TypeofExprAST::Generate()
// {
// 	dinfo.emitLocation(this);
// 	auto val = arg->Generate();
// 	return builder.CreateLoad(builder.CreateGEP(val, {builder.getInt32(0),builder.getInt32(0)}));
// }

llvm::Value * Birdee::TryBlockAST::Generate()
{
	dinfo.emitLocation(this);
	if (!gen_context.personality_func)
	{
		gen_context.personality_func = Function::Create(
			FunctionType::get(builder.getInt32Ty(), true), 
			llvm::GlobalValue::ExternalLinkage, "__Birdee_Personality", module);
		gen_context.begin_catch_func = Function::Create(
			FunctionType::get(builder.getInt8PtrTy(), { builder.getInt8PtrTy() }, false),
			llvm::GlobalValue::ExternalLinkage, "__Birdee_BeginCatch", module);
	}
	helper.cur_llvm_func->setPersonalityFn(gen_context.personality_func);
	helper.cur_llvm_func->addFnAttr(llvm::Attribute::UWTable);
	auto old_landing = gen_context.landingpad;
	BasicBlock* landing = BasicBlock::Create(context, "landingpad", helper.cur_llvm_func);
	BasicBlock* cont = BasicBlock::Create(context, "trycont", helper.cur_llvm_func);
	gen_context.landingpad = landing;
	bool has_return = try_block.Generate();
	if (!has_return)
	{
		builder.CreateBr(cont);
	}
	gen_context.landingpad = old_landing;
	builder.SetInsertPoint(landing);
	if (!gen_context.landingpadtype)
		gen_context.landingpadtype = llvm::StructType::get(context,
			{ builder.getInt8PtrTy(),builder.getInt32Ty() });
	auto pad = builder.CreateLandingPad(gen_context.landingpadtype, catch_variables.size());
	auto expobj = builder.CreateExtractValue(pad, 0 , "expobj");
	auto switchnum = builder.CreateExtractValue(pad, 1, "expswitch");
	vector<BasicBlock*> catches;
	assert(catch_variables.size());
	catches.reserve(catch_variables.size());
	for (int i = 0; i < catch_variables.size(); i++)
	{
		string catch_block_name = "catch.";
		auto& var = catch_variables[i];
		assert(var->resolved_type.type == tok_class && var->resolved_type.class_ast->needs_rtti);
		catch_block_name += var->resolved_type.class_ast->GetUniqueName();
		catches.push_back(BasicBlock::Create(context, catch_block_name, helper.cur_llvm_func, cont));
		auto rtti_itr = gen_context.rtti_map.find(var->resolved_type.class_ast);
		assert(rtti_itr != gen_context.rtti_map.end());
		pad->addClause(rtti_itr->second);
	}

	//still in landingpad, jump to the first catch block
	builder.CreateBr(catches[0]);

	for (int i = 0; i < catch_variables.size(); i++)
	{
		auto cur_bb = catches[i];
		auto& var = catch_variables[i];
		builder.SetInsertPoint(cur_bb);
		auto func = llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::eh_typeid_for);
		auto catch_ty_id = builder.CreateCall(func, builder.CreatePointerCast(pad->getClause(i), builder.getInt8PtrTy()));
		auto should_catch = builder.CreateICmpEQ(switchnum, catch_ty_id);
		BasicBlock* not_caught_block, *caught_block;
		if (i != catch_variables.size() - 1) //if current catch block is not the last one
			not_caught_block = catches[i + 1];//jump to the next block if not caught
		else
			not_caught_block = BasicBlock::Create(context, "try.resume", helper.cur_llvm_func, cont);
		caught_block = BasicBlock::Create(context, cur_bb->getName() + ".caught", helper.cur_llvm_func, not_caught_block);
		builder.CreateCondBr(should_catch, caught_block, not_caught_block);

		builder.SetInsertPoint(caught_block);
		var->Generate();
		Value* obj = builder.CreateCall(gen_context.begin_catch_func, expobj);
		obj = builder.CreatePointerCast(obj, var->resolved_type.class_ast->llvm_type->getPointerTo());
		builder.CreateStore(obj,var->llvm_value);
		bool isterminator = catch_blocks[i].Generate();
		if (!isterminator)
			builder.CreateBr(cont);

		if (i == catch_variables.size() - 1) //if current catch block is the last one, generate "resume" instruction
		{
			builder.SetInsertPoint(not_caught_block);
			builder.CreateResume(pad);
		}
	}

	builder.SetInsertPoint(cont);
	return nullptr;
}

llvm::Value* Birdee::ThrowAST::Generate()
{
	auto v = expr->Generate();
	if(!gen_context.throw_func)
		gen_context.throw_func = Function::Create(
			FunctionType::get(builder.getVoidTy(), { builder.getInt8PtrTy() }, false),
			llvm::GlobalValue::ExternalLinkage, "__Birdee_Throw", module);
	DoGenerateCall(gen_context.throw_func, {builder.CreatePointerCast(v,builder.getInt8PtrTy())});
	builder.CreateUnreachable();
	return nullptr;
}


