from bdutils import *
import inspect

def const_uint(i):
	assert( isinstance(i,int))
	return lambda:set_uint(i)

def const_int(i):
	assert( isinstance(i,int))
	return lambda:set_int(i)

def define(name, value):
	glb = inspect.stack()[1][0].f_globals
	glb["_"+name] = value
	glb[name] = const_uint(value)

def define_signed(name, value):
	glb = inspect.stack()[1][0].f_globals
	glb["_"+name] = value
	glb[name] = const_int(value)