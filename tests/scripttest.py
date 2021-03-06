from birdeec import *
from bdtesting.utils import * 
import os

set_print_ir(False)

print("The OS name is ", get_os_name(), ". The target bit width is ", get_target_bits())

assert_ok('''
func add(a as int,
	b as int) as int
	return a+b
end

add(
	12,
	3)
''')

assert_ok('''
{@
annotated=[]
def some(f):
	print(f.proto.name)
	annotated.append(f.proto.name)
@}
class AAA
	@some
	public func asd()
	end

	@some
	public func asd2[T]()
	end
end

dim t = new AAA
t.asd2[int]()

{@
assert(annotated[0]=="asd")
assert(annotated[1]=="asd2[int]")
@}
''')

assert_fail('''
{@def some(f):
	print(f.proto.name)@}

class AAA
	@some
	abstract func bbb()
end
@}
''')

assert_fail('''
{@def some(f):
	print(f.proto.name)@}

class AAA
	@some
	public a as int
end
@}
''')

assert_generate_ok('''
class AAA
	@volatile
	public v as int
end

dim t = new AAA
t.v=1
dim t2=t.v

@volatile
dim t3 as int

t3=12
dim g=t3
g=23
''')

top_level('''import vector''')
v=expr("birdee")
mod=v.get().resolved_type.get_detail()
assert(len(mod.get_submodules())==0)
mod=mod.mod
assert(len(mod.get_classmap())!=0)
clear_compile_unit()

###Auto-complete Test
try:
	top_level('''
	dim p as string
	println(p.:)
	''')
	process_top_level()
except CompileException:
	pass
assert(get_auto_completion_ast()!=None)
clear_compile_unit()
assert(get_auto_completion_ast()==None)
#####

####Test complicated source & clearing
srcpath=os.path.join(os.environ["BIRDEE_HOME"], "src", "serialization", "json", "deserializer.bdm")
print("Test source path", srcpath)
with open(srcpath) as f:
	src=f.read()
top_level(src)
process_top_level()
clear_compile_unit()

top_level(src)
process_top_level()
clear_compile_unit()
######done

assert_fail("import a.b.c")

top_level("dim a=1")
CallExprAST.new(expr("println"), [expr('"hello"')])
process_top_level()
generate()
clear_compile_unit()

#top-level defer test
assert_generate_ok('''
println("AAAA")
defer
	println("BBBB")
end
dim c= int2str(12)
defer
	println("BBBB")
end
dim d = bool2str(true)
''')

assert_generate_ok('''
struct AAA
	public a as int
	public func b[T]() as string
	end
end

func c() as AAA
end

c().b[int]()
''')

assert_generate_ok('''
function aaa()
	defer
		println("SSSS")
	end
end
''')

assert_generate_fail('''
function aaa()
	try
	catch e as runtime_exception
		println("HAHA")
	end
end
''')

assert_fail('''
struct A
	public b as int
end
func fff() as A 
	dim v as A
	return v
end
fff().b=1
''')

assert_ok('''
struct A
	public b as int
end
func fff() as A 
	dim v as A
	return v
end
dim v = fff().b
''')

assert_generate_ok('''
class meme
	private func say()=>println("hi")
end
dim v = new meme
dim clo = {@
cls = resolve_type("meme").get_detail()
memb = MemberExprAST.new_func_member(expr("v"),cls.funcs[0])
set_ast(memb)
@}
clo()
''')

assert_generate_ok('''
dim a=int2str(1)+pointer2str(pointerof(null))+double2str(3.0)
a.get_raw()
breakpoint()
''')

assert_fail('''
__create_basic_exception_no_call(1)
''')

assert_fail('''
birdee.__create_basic_exception_no_call(1)
''')

assert_generate_ok('''
func get() as int => 3
dim a as int[] = [1,2,get()]

closure theclosure() as int
dim f1 as theclosure
dim b as theclosure[] = [f1, get]

class A
end

class B:A
end

class C:A
end
dim c as A[] = [new B, new C]

dim d as int[][]=[[1,2,3],[4,5,6],]
''')

assert_generate_ok('''
import rtti:dyn_cast

@enable_rtti
class parent
end

class child:parent
end

dim a = new child
dim c as parent =a
a= dyn_cast[child](c)
''')


assert_generate_ok('''
class parent
	public func __init__(v as string)=> this.v=v
	public v as string
	@virtual public func __add__(other as parent) as parent => new parent(v+other.v)
end

class child:parent
	public func __init__(v as string)=> this.v=v
	public func __add__(other as child) as child => new child(v + other.v + "child")
	public func add(other as child) as parent => super.__add__(other)
end

dim a = new child("1"), b= new child("2")
dim c as parent =a, d as parent =b
dim e = c+d

''')

#tests for member function templates
assert_generate_ok('''
class tttt
	public func a[T1,T2,T3](v1 as T2, v2 as T3)
	end
	public func b[T1](v1 as T1)
	end
end

dim p as tttt =  new tttt
p.a[int,float,double](2.3,4.5)
p.a[int]("s","b")
p.b[int](1)
p.b(1.4)
''')

#test for binding member functions
assert_generate_ok('''
dim a = "asa".length
dim b as closure () as uint = "asa".length
''')

#test for "too many template arguments"
assert_fail('''
function v[T]()
end
v[int,float]()
''')

assert_ok('''
{@set_ast(stmt("declare function getc() as int"))@}
getc()
''')


assert_ok('''
{@from bdutils import *
from traits import *
from traits import _
def print_ast(node):
	print(node)
@}
dim a = {@set_int(234)@}
dim b = {@set_str("hello")@}
dim c as {@resolve_set_type("int")@}

@require((is_template_inst, _), (is_type_templ_arg_in_class, _, 0))
struct p[...]
	public v1 as {@cls_templ_type_at(0)@}
	public v2 as {@cls_templ_type_at(1)@}
end

dim pv as p[int,string]

@print_ast pv.v1=1
@print_ast
pv.v2="str"
''')

assert_ok('''
{@from bdutils import *@}
dim a = {@set_int(234)@}
dim b = {@set_str("hello")@}
dim c as {@resolve_set_type("int")@}
struct p[...]
	public v1 as {@cls_templ_type_at(0)@}
	public v2 as {@cls_templ_type_at(1)@}
end
dim pv as p[int,string]
''')


assert_fail('''
{@from bdutils import *@}
struct p[...]
	public v1 as {@cls_templ_type_at(10)@}
	public v2 as {@cls_templ_type_at(1)@}
end
dim pv as p[int,string]
''')

assert_ok('''
dim a as string[] = new string *32
dim b as {@set_type(resolve_type("int"))@}[] =new int * 32
''')

assert_ok('''
import variant:variant
dim v as variant[{@set_type(resolve_type("int"))@}]
''')

assert_generate_ok('''
class clsbase [Arg as int, T1,T2]
	public a as T1
	public b as T2
end

class clsderived [T1] : clsbase[{@set_ast(NumberExprAST.new(BasicType.INT,32))@}, T1,{@set_type(resolve_type("float"))@}]
	public c as T1
end

dim a = new clsderived[int]
''')

assert_ok('''
import variant:variant
dim v as variant[byte,int,long,string]
v.set(1)
v.set(2L)
v.set("1234")
println(v.get[string]())
''')

assert_ok('''
{@assert(size_of(resolve_type("string"))==get_target_bits()/8)@}
{@assert(size_of(resolve_type("long"))==8)@}
''')


assert_generate_ok('''
import unsafe:*
dim a as pointer
dim b as string = ptr_cast[string](a)
dim c as int[] = ptr_cast[int[]](b)
dim d as ulong = ptr_cast[ulong](c)
dim e as uint = d
c = ptr_cast[int[]](e)
a = ptr_cast[pointer](c)
d = ptr_cast[ulong](e)

d = ptr_load[ulong](a)
b = ptr_load[string](a)

ptr_store(a,d)
ptr_store(a,b)

b = bit_cast[string](a)
b = bit_cast[string](d)

dim k = ptr_load[ulong]
'''
)

assert_generate_fail('''
import unsafe:*
dim b as string
dim e as uint
b = bit_cast[string](e)
''')

assert_ok('''
class a[T]
end
dim aaa as a[int][] = new a[int] * 10
aaa[0]=new a[int]
dim ab = aaa[0]

class Cb[T]
	public bb as T[]
	public cc as T
end

dim bbbb as Cb[int[]]
dim ccccc as int[][]=bbbb.bb
dim ddddd as int[]=bbbb.cc
'''
)

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
import hash:hash_map

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
#generate()
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

ddd="'''"
assert_ok('''
dim a = {ddd}asd


\n\a
{ddd}, b as int
print(a)'''.format(ddd=ddd))


#tests for python script scopes
assert_ok('''
{@
a=123
@}
func bbb() as int
	{@print("global a = ",a)@}
	{@b=321@}
	{@assert(b==321)@}
	if 1==1 then
		if 1==1 then
			{@assert(b==321)@}
		end
	end
	{@global a
a=111@}
end
{@assert(a==111)
assert('b' not in globals())@}
''')

#tests on struct
assert_ok('''
struct complex
	public a as double
	public b as double
	public func set(real as double, img as double)
		a=real
		b=img
	end
	public func __add__(other as complex) as complex
		dim ret as complex
		ret.a=a+other.a
		ret.b=b+other.b
		return ret
	end
	public func tostr() as string => "complex(" + int2str(a) + "," + int2str(b) + ")"
end

func mkcomplex(real as double,img as double) as complex
		dim ret as complex
		ret.a=real
		ret.b=img
		return ret
end

dim a = mkcomplex(1,2)
dim b = mkcomplex(3,4)
dim c = a+b
print((a+c).tostr())
dim d = (b+c).a
dim e = addressof(a.b)
''')

assert_ok('''
closure fff() as complex
struct complex
	public a as double
	public b as double
	public func get_closure() as fff
		return func () as complex => this
	end

end

dim x as complex
dim y = x.get_closure()()''')

assert_fail('''
struct complex
	public a as double
	public b as double
end
func mkcomplex() as complex
end
dim x = addressof(mkcomplex().a)''')

assert_ok('''
@enable_rtti
class someclass

end

dim a as someclass = new someclass
println(typeof(a).get_name())
'''
)



print("All tests done!")