from birdeec import *
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