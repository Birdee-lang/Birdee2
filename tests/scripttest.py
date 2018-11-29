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


#tests for one-line function
top_level(
'''
function func1(a as int, b as int) as int => a+b
function () => func1(1,2)
'''
)
process_top_level()
generate()
clear_compile_unit()

#test for closure assignment
try:
	top_level(
'''
closure cii_i(a as int,b as int) as int
functype fii_i(a as int,b as int) as int

dim a as cii_i, b as fii_i
b=a
'''
)
	process_top_level()
	assert(False)
except CompileException:
	e=get_compile_error()
	print(e.linenumber,e.pos,e.msg)
clear_compile_unit()


#test for PrototypeType parser
top_level(
'''
function apply1(f as closure (a as int), b as int) =>  f(b)
function apply2(f as closure (a as int) as int, b as int) as int =>  f(b)
function apply3(f as functype (a as int), b as int) =>  f(b)

apply1(function (a as int) => println(int2str(a)), 32 )
apply2(function (a as int) as int => a+2, 32 )
apply3(function (a as int) => println(int2str(a)), 32 )
'''
)
process_top_level()
clear_compile_unit()