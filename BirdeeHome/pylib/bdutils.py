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
	
def set_char(c):
	assert(isinstance(c,str) and len(c)==1)
	set_int(ord(c))

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

def set_expr(s):
	set_ast(expr(s))

def set_stmt(s):
	set_ast(stmt(s))

def resolve_set_type(s):
	set_type(resolve_type(s))


#utilities for templates
def get_cls_templ_at(idx):
	thecls = get_cur_class()
	require_(is_template_inst(thecls), index_within(idx,0,len(thecls.template_instance_args)))
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
def get_func_arg_at(func,idx):
	args = func.proto.args
	require_(index_within(idx,0,len(args)))
	return args[idx]


def get_func_templ_at(idx, thefunc = None):
	if not thefunc:
		thefunc = get_cur_func()
	require_(is_template_inst(thefunc), index_within(idx,0,len(thefunc.template_instance_args)))
	return thefunc.template_instance_args[idx]

def get_func_type_templ_at(idx, thefunc = None):
	if not thefunc:
		thefunc = get_cur_func()
	arg = get_func_templ_at(idx, thefunc)
	require_(is_type_templ_arg(arg,idx))
	return arg.resolved_type

def get_func_expr_templ_at(idx, thefunc = None):
	if not thefunc:
		thefunc = get_cur_func()
	arg = get_func_templ_at(idx, thefunc)
	require_(is_expr_templ_arg(arg,idx))
	return arg.expr

def func_templ_type_at(idx, thefunc = None):
	if not thefunc:
		thefunc = get_cur_func()
	set_type(get_func_type_templ_at(idx, thefunc))

def func_templ_expr_at(idx, thefunc = None):
	if not thefunc:
		thefunc = get_cur_func()
	set_ast(get_func_expr_templ_at(idx, thefunc))


'''
T should be a resolved type.
callback should be (idx, length, field)
'''
def foreach_field(T, callback):
	if not is_a_class(T) and not is_a_struct(T):
		raise RuntimeError("T = {} should be a class or struct".format(T))
	T=T.get_detail()
	length=len(T.fields)
	for idx, field in enumerate(T.fields):
	    callback(idx, length, field)