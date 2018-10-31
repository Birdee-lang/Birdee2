from birdeec import *
top_level('''
dim a as string
println(a)
{@
print("hello world")
@}
''')
process_top_level()
clear_compile_unit()

try:
	top_level('''
	println(a@#!@#@
	{@
	print("hello world")
	@}
	''')
	process_top_level()
	assert(False)
except TokenizerException:
	e=get_tokenizer_error()
	print(e.linenumber,e.pos,e.msg)
clear_compile_unit()

try:
	top_level('''
import hash_map:hash_map

dim map = new hash_map
	''')
	process_top_level()
	assert(False)
except CompileException:
	e=get_compile_error()
	print(e.linenumber,e.pos,e.msg)
clear_compile_unit()



top_level('''
dim a =true
dim b =1
while a
	a= b%2==0
end while
	''')
process_top_level()
clear_compile_unit()

top_level(
'''
dim a = {@set_ast(expr("32"))@}
println(int2str(a))

{@
set_ast(stmt(\'''if a==3 then
	a=4
end\'''))
@}

dim c as {@set_type(resolve_type("int"))@}
c=a
'''
)
process_top_level()
clear_compile_unit()


#test for constructor: private constructor
try:
	top_level(
'''
class AAA
	function __init__()
	end

end

dim a = new AAA
'''
	)
	process_top_level()
	assert(False)
except CompileException:
	e=get_compile_error()
	print(e.linenumber,e.pos,e.msg)
clear_compile_unit()

#test for constructor: wrong number of arguments
try:
	top_level(
'''
class AAA
	public function __init__(a as int)
	end

end

dim a = new AAA
'''
	)
	process_top_level()
	assert(False)
except CompileException:
	e=get_compile_error()
	print(e.linenumber,e.pos,e.msg)
clear_compile_unit()

