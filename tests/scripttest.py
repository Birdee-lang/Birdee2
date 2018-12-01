from birdeec import *

def assert_fail(istr):
	try:
		top_level(istr)
		process_top_level()
		assert(False)
	except TokenizerException:
		e=get_tokenizer_error()
		print(e.linenumber,e.pos,e.msg)
	except CompileException:
		e=get_compile_error()
		print(e.linenumber,e.pos,e.msg)		
	clear_compile_unit()

def assert_ok(istr):
	top_level(istr)
	process_top_level()
	clear_compile_unit()

assert_ok('''
dim a as string
println(a)
{@
print("hello world")
@}
''')

assert_fail('''
	println(a@#!@#@
	{@
	print("hello world")
	@}
	''')

assert_fail('''
import hash_map:hash_map

dim map = new hash_map
	''')



assert_ok('''
dim a =true
dim b =1
while a
	a= b%2==0
end while
	''')

assert_ok(
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


#test for constructor: private constructor
assert_fail(
'''
class AAA
	function __init__()
	end

end

dim a = new AAA
''')

#test for constructor: wrong number of arguments
assert_fail(
'''
class AAA
	public function __init__(a as int)
	end

end

dim a = new AAA
''')


#tests for one-line function
assert_ok(
'''
function func1(a as int, b as int) as int => a+b
function () => func1(1,2)
'''
)

#test for closure assignment
assert_fail(
'''
closure cii_i(a as int,b as int) as int
functype fii_i(a as int,b as int) as int

dim a as cii_i, b as fii_i
b=a
'''
)


#test for PrototypeType parser
assert_ok(
'''
function apply1(f as closure (a as int), b as int) =>  f(b)
function apply2(f as closure (a as int) as int, b as int) as int =>  f(b)
function apply3(f as functype (a as int), b as int) =>  f(b)

apply1(function (a as int) => println(int2str(a)), 32 )
apply2(function (a as int) as int => a+2, 32 )
apply3(function (a as int) => println(int2str(a)), 32 )
'''
)

# tests for template parameters derivation
top_level(
'''
	class testc
		public function add[T](a as T) as T
			return a+39
		end
		public function add2[T1, T2](a as T1, b as T2) as T1
			if (b) then
				return a+10
			else 
				return a+20
			end
		end
	end
	function add3[T](a as T) as T
		return a + 49
	end
	function add4[T1, T2](a as T1, b as T2) as T1
		if (b) then
			return a+30
		else 
			return a+40
		end
	end

	dim tc = new testc
	tc.add[int](10)
	tc.add(10)
	tc.add[float](20.0)
	tc.add(20.0)
	tc.add2[int, boolean](20, true)
	tc.add2[float, boolean](20.0, true)
	tc.add2(30, true)
	tc.add2(30.0, true)
	add3[int](40)
	add3(40)
	add3[float](40.0)
	add3(40.0)
	add4[int, boolean](40, true)
	add4[float, boolean](40.0, true)
	add4(40, true)
	add4(40.0, true)
	''')
process_top_level()
generate()
clear_compile_unit()

# tests for failed template parameters derivation
assert_ok('''
	function add4[T1, T2](a as T1, b as T2, c as string) as T1
		return a
	end
	add4(1,1.2,"hi")''')

assert_fail('''
function add3[T1, T2](a as T1, b as T2, c as T2) as T1
	return a
end
add3(1,2,"2")''')

assert_fail('''
function add2[T1, T2](a as T1, b as string) as T1
	return a
end
add2(1,"2")''')

assert_fail('''
function add2[T1, v as int](a as T1, b as string) as T1
	return a
end
add2(1,"2")''')
