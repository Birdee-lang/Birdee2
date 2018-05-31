#include "CastAST.h"
#include "llvm/IR/IRBuilder.h"
#include "TokenDef.h"
using namespace llvm;
using namespace Birdee;
extern IRBuilder<> builder;

template<Token typefrom, Token typeto>
Value* NumberCast(Value* v)
{
	builder.CreateSExtOrTrunc;
	builder.CreateZExtOrTrunc; builder.CreateFPCast;
	return nullptr;
}

template<Token typefrom, Token typeto>
llvm::Value * Birdee::CastNumberExpr<typefrom, typeto>::Generate()
{
	return NumberCast<typefrom, typeto>(expr->Generate());
}


#define GenerateCastSame(from,to) template<> \
Value* NumberCast<from,to>(Value* v)\
{\
	return v;\
}\
template class Birdee::CastNumberExpr<from, to>;

#define GenerateCastFP2I(from,to,method,toty) template<> \
Value* NumberCast<from,to>(Value* v)\
{\
	return builder.method(v, builder.toty());\
}\
template class Birdee::CastNumberExpr<from, to>;

GenerateCastFP2I(tok_int, tok_float, CreateSIToFP, getFloatTy);
GenerateCastFP2I(tok_long, tok_float, CreateSIToFP, getFloatTy);
GenerateCastFP2I(tok_int, tok_double, CreateSIToFP, getDoubleTy);
GenerateCastFP2I(tok_long, tok_double, CreateSIToFP, getDoubleTy);

GenerateCastFP2I(tok_uint, tok_float, CreateUIToFP, getFloatTy);
GenerateCastFP2I(tok_ulong, tok_float, CreateUIToFP, getFloatTy);
GenerateCastFP2I(tok_uint, tok_double, CreateUIToFP, getDoubleTy);
GenerateCastFP2I(tok_ulong, tok_double, CreateUIToFP, getDoubleTy);

GenerateCastFP2I(tok_double, tok_int, CreateFPToSI, getInt32Ty);
GenerateCastFP2I(tok_double, tok_long, CreateFPToSI, getInt64Ty);
GenerateCastFP2I(tok_float, tok_int, CreateFPToSI, getInt32Ty);
GenerateCastFP2I(tok_float, tok_long, CreateFPToSI, getInt64Ty);

GenerateCastFP2I(tok_double, tok_uint, CreateFPToUI, getInt32Ty);
GenerateCastFP2I(tok_double, tok_ulong, CreateFPToUI, getInt64Ty);
GenerateCastFP2I(tok_float, tok_uint, CreateFPToUI, getInt32Ty);
GenerateCastFP2I(tok_float, tok_ulong, CreateFPToUI, getInt64Ty);

GenerateCastFP2I(tok_float, tok_double, CreateFPCast, getDoubleTy);
GenerateCastFP2I(tok_double, tok_float, CreateFPCast, getFloatTy);

GenerateCastSame(tok_int, tok_uint);
GenerateCastSame(tok_long, tok_ulong);

GenerateCastSame(tok_uint, tok_int);
GenerateCastSame(tok_ulong, tok_long);

GenerateCastSame(tok_ulong, tok_ulong);
GenerateCastSame(tok_long, tok_long);
GenerateCastSame(tok_uint, tok_uint);
GenerateCastSame(tok_int, tok_int);
GenerateCastSame(tok_float, tok_float);
GenerateCastSame(tok_double, tok_double);

GenerateCastFP2I(tok_int, tok_long, CreateSExtOrTrunc, getInt64Ty);
GenerateCastFP2I(tok_int, tok_ulong, CreateZExtOrTrunc, getInt64Ty);
GenerateCastFP2I(tok_uint, tok_long, CreateZExtOrTrunc, getInt64Ty);
GenerateCastFP2I(tok_uint, tok_ulong, CreateZExtOrTrunc, getInt64Ty);

GenerateCastFP2I(tok_long, tok_int, CreateSExtOrTrunc, getInt32Ty);
GenerateCastFP2I(tok_long, tok_uint, CreateZExtOrTrunc, getInt32Ty);
GenerateCastFP2I(tok_ulong, tok_int, CreateZExtOrTrunc, getInt32Ty);
GenerateCastFP2I(tok_ulong, tok_uint, CreateZExtOrTrunc, getInt32Ty);

