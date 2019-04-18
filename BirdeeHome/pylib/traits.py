from birdeec import *

_ = []

def get_boolean_type():
	a = ResolvedType()
	a.set_detail(BasicType.BOOLEAN,None)
	return a

def require_(*checks):
	for i in checks:
		if not i[0]:
			raise RuntimeError(i[1]())

def require(*checks):
	def ret(curcls):
		for pack in checks:
			func = pack[0]
			args = [0] * (len(pack) - 1)
			for i in range(1,len(pack)):
				if id(pack[i])==id(_):
					args[i-1] = curcls
				else:
					args[i-1] = pack[i]
			result = func(*args)
			if not result[0]:
				raise RuntimeError(result[1]())
	return ret 

def	NOT(func, *args):
	def not_func(*subargs):
		result, str_func = func(*subargs)
		def not_str_func():
			return "Not({})".format(str_func())
		return (not result, not_str_func)
	return (not_func,) + args 

def	AND(pack1, pack2):
	func1 = pack1[0]
	func2 = pack2[0]
	split = len(pack1) - 1 # the number of arguments in first function
	def and_func(*subargs):
		args1 = subargs[0: split] #args for first func
		args2 = subargs[split: ]
		result1, str_func1 = func1(*args1)
		result2, str_func2 = func2(*args2)
		def and_str_func():
			return "And({} , {})".format(str_func1(), str_func2())
		return (result1 and result2, and_str_func)
	return (and_func,) + pack1[1:] + pack2[1:]


def	OR(pack1, pack2):
	func1 = pack1[0]
	func2 = pack2[0]
	split = len(pack1) - 1 # the number of arguments in first function
	def or_func(*subargs):
		args1 = subargs[0: split] #args for first func
		args2 = subargs[split: ]
		result1, str_func1 = func1(*args1)
		result2, str_func2 = func2(*args2)
		def or_str_func():
			return "Or({} , {})".format(str_func1(), str_func2())
		return (result1 or result2, and_str_func)
	return (or_func,) + pack1[1:] + pack2[1:]	

def is_cls_template_inst(scls):
	return (scls.is_template_instance(), lambda:"The input class {} is expected to be a template instance".format(scls.get_unique_name()) )

def index_within(index, st, ed):
	return (st <= index <ed, lambda:"The index {index} is expected to be {st}<= index < {ed}".format(index=index, st= st, ed= ed) )

def is_class(node):
	return (isinstance(node,ClassAST), lambda:"The AST node {} is expected to be a ClassAST".format(str(node)) )

def is_cls_template(scls):
	return (scls.is_template(), lambda:"The input class {} is expected to be a template".format(scls.get_unique_name()) )

def is_type_templ_arg_in_class(clazz,idx):
	require_(is_cls_template_inst(clazz), index_within(idx, 0, len(clazz.template_instance_args)))
	arg = clazz.template_instance_args[idx]
	return (arg.kind==TemplateArgument.TemplateArgumentType.TEMPLATE_ARG_TYPE , 
		lambda:"The {}-th template argument of class {} is expected to be a type".format(idx, clazz.get_unique_name()))

def is_type_templ_arg(arg,idx):
	return (arg.kind==TemplateArgument.TemplateArgumentType.TEMPLATE_ARG_TYPE , 
		lambda:"The {}-th template argument is expected to be a type".format(idx))

def is_expr_templ_arg(args,idx):
	return (args[idx].kind==TemplateArgument.TemplateArgumentType.TEMPLATE_ARG_EXPR , 
		lambda:"The {}-th template argument is expected to be an expression".format(idx))

'''
Check if "type" is a prototype. Throw if not.
 - type: The ResolvedType to check
'''
def is_prototype(ctype):
	return (ctype.base==BasicType.FUNC and ctype.index_level==0,
		lambda:"The type {} is not a function prototype".format(str(ctype)) )

def is_prototype_(ctype):
	return ctype.base==BasicType.FUNC and ctype.index_level==0

'''
Get return type of an expression or single variable definition.
 - expr: the expression or single variable definition to be processed.
	It must have field "resolved_type". The resolved_type must be be a function prototype

Returns the resolvedtype of what the function returns
'''
def return_type_of(expr):
	proto = expr.resolved_type
	require_(is_prototype_(proto))
	return proto.get_detail().return_type

'''
Get the i-th parameter definition of the current function
 - i: the parameter index, starting from 0

Returns the single variable definition of the parameter
'''
def get_parameter(i):
	func = get_cur_func()
	assert(func)
	args=func.proto.args
	length=len(args)
	assert(0<=i<length)
	return args[i]

'''
Checks if the PrototypeAST has the expected definition
 - proto: the PrototypeAST to be checked
 - returns: the expected return type
 - args: the expected parameters types

Returns True is proto is defined as expected
'''
def check_prototype(proto, returns, *args):
	if proto.return_type!=returns:
		return False
	parg=proto.args
	if len(parg)!=len(args):
		return False
	for i in range(len(args)):
		if parg[i].resolved_type!=args[i]:
			return False
	return True

'''
Checks if the PrototypeAST has the expected definition
 - proto: the PrototypeAST to be checked
 - returns: the expected return type
 - args: the expected parameters types

Throw an RuntimeError if proto is not expected 
'''
def check_prototype_throw(proto, returns, *args):
	if not check_prototype(proto,returns,*args):
		raise RuntimeError("The prototype {} does not match expected the prototype".format(str(proto)))
