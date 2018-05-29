
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

#include "CompileError.h"
#include "AST.h"
#include <cassert>

using namespace llvm;
using Birdee::CompileError;

static LLVMContext context;
static IRBuilder<> builder(context);

static std::unique_ptr<Module> module;
static std::unique_ptr<DIBuilder> DBuilder;

static Type* ty_int=IntegerType::getInt32Ty(context);
static Type* ty_long = IntegerType::getInt64Ty(context);

struct DebugInfo {
	DICompileUnit *TheCU;
	DIType *DblTy;
	std::vector<DIScope *> LexicalBlocks;

	void emitLocation(Birdee::ExprAST *AST);
	DIType *getDoubleTy();
} KSDbgInfo;

void Birdee::CompileUnit::InitForGenerate()
{
	module= llvm::make_unique<Module>(name, context);

	DBuilder = llvm::make_unique<DIBuilder>(*module);
	KSDbgInfo.TheCU = DBuilder->createCompileUnit(
		dwarf::DW_LANG_C, DBuilder->createFile(filename, "."),
		"Birdee Compiler", 0, "", 0);



	// Finalize the debug info.
	DBuilder->finalize();

	// Print out all of the generated code.
	module->print(errs(), nullptr);
}


Value * Birdee::NumberExprAST::Generate()
{
	switch (Val.type)
	{
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
	Value* ret = Val->Generate();
	return builder.CreateRet(ret);
}