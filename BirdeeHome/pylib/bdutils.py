from birdeec import *
from traits import *

#utilities for making AST/type
def make_int(n):
	return NumberExprAST.new(BasicType.INT,n)
def set_int(n):
	set_ast(make_int(n))
def make_uint(n):
	return NumberExprAST.new(BasicType.UINT,n)
def set_uint(n):
	set_ast(make_uint(n))

def make_long(n):
	return NumberExprAST.new(BasicType.LONG,n)
def set_long(n):
	set_ast(make_long(n))
def make_ulong(n):
	return NumberExprAST.new(BasicType.ULONG,n)
def set_ulong(n):
	set_ast(make_ulong(n))

def make_float(n):
	return NumberExprAST.new(BasicType.FLOAT,n)
def set_float(n):
	set_ast(make_float(n))
def make_double(n):
	return NumberExprAST.new(BasicType.DOUBLE,n)
def set_double(n):
	set_ast(make_double(n))

def make_bool(n):
	return BoolLiteralExprAST.new(n)
def set_bool(n):
	set_ast(make_bool(n))

def make_str(s):
	return StringLiteralAST.new(s)
def set_str(s):
	set_ast(make_str(s))

def resolve_set_type(s):
	set_type(resolve_type(s))


#utilities for templates
def get_cls_templ_at(idx):
	thecls = get_cur_class()
	require_(is_cls_template_inst(thecls), index_within(idx,0,len(thecls.template_instance_args)))
	return thecls.template_instance_args[idx]

def get_cls_type_templ_at(idx):
	arg = get_cls_templ_at(idx)
	require_(is_type_templ_arg(arg,idx))
	return arg.resolved_type

def get_cls_expr_templ_at(idx):
	arg = get_cls_templ_at(idx)
	require_(is_expr_templ_arg(arg,idx))
	return arg.expr

def cls_templ_type_at(idx):
	set_type(get_cls_type_templ_at(idx))

def cls_templ_expr_at(idx):
	set_ast(get_cls_expr_templ_at(idx))

#utilities for functions
def get_function_arg_at(func,idx):
	args = func.proto.args
	require_(index_within(idx,0,len(args)))
	return args[idx]
