from birdeec import *

def resolved_type():
	top_level('''
dim a as string
function func () as int
end
dim b = func
dim v as int[]
''')
	process_top_level()
	top=get_top_level()
	assert(top[0].resolved_type.base==BasicType.CLASS)
	clazz = top[0].resolved_type.get_detail()
	assert(isinstance(clazz,ClassAST))
	assert(clazz.get_unique_name()=="birdee.string")

	assert(top[2].resolved_type.base==BasicType.FUNC)
	proto = top[2].resolved_type.get_detail()
	assert(isinstance(proto,PrototypeAST))

	assert(top[3].resolved_type.base==BasicType.INT)
	assert(top[3].resolved_type.get_detail()==None)
	assert(top[3].resolved_type.index_level==1)

	top[3].resolved_type.set_detail(BasicType.CLASS,clazz)




def return_ast():
	top_level('''
{@
def return_123(func):
	for stmt in func.body:
		if isinstance(stmt,ReturnAST):
			print(stmt.expr)
			newv=NumberExprAST.new(BasicType.FLOAT,1024)
			print(newv.get())
			stmt.expr=newv

def check_return(func):
	for stmt in func.body:
		if isinstance(stmt,ReturnAST):
			assert(stmt.expr.type==BasicType.FLOAT and stmt.expr.value==1024.0)
				
@}

@return_123
@check_return
function func() as float
	return 1
end
''')
	process_top_level()



def prototype_ast():
	top_level('''
function func (a as string, b as int) as int
end

class clz
	function func (a as string, b as int) as int
	end
end

''')
	process_top_level()
	top=get_top_level()
	assert(len(top[0].proto.args)==2)
	ty=top[0].proto.args[0].resolved_type
	assert(ty.base==BasicType.CLASS and ty.get_detail().get_unique_name()=="birdee.string")
	ty=top[0].proto.args[1].resolved_type
	assert(ty.base==BasicType.INT)

	clz=top[1]
	func_proto=clz.funcs[0].decl.proto
	assert(id(func_proto.cls)==id(clz))
	func_proto.cls=clz
	assert(id(func_proto.cls)==id(clz))


def resolve_type_test():
	top_level('''
dim a = "sada"
class clz
end
class lst[T]
end
''')
	process_top_level()

	assert(get_top_level()[0].resolved_type==resolve_type("birdee.string"))
	assert(str(resolve_type("lst[int]"))=="ast_write_test.lst[int]")
	assert(str(resolve_type("clz"))=="ast_write_test.clz")

resolve_type_test()
#return_ast()
#resolved_type()
#prototype_ast()
