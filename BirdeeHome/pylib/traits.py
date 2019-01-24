from birdeec import *

'''
Check if "type" is a prototype. Throw if not.
 - type: The ResolvedType to check
'''
def check_is_prototype(ctype):
	if ctype.base!=BasicType.FUNC or ctype.index_level!=0:
		raise RuntimeError("The type {} is not a function prototype".format(str(ctype)))

'''
Get return type of an expression or single variable definition.
 - expr: the expression or single variable definition to be processed.
	It must have field "resolved_type". The resolved_type must be be a function prototype

Returns the resolvedtype of what the function returns
'''
def return_type_of(expr):
	proto = expr.resolved_type
	check_is_prototype(proto)
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
		if parg[i]!=args[i]:
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