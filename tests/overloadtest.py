from bdtesting.utils import *
#import birdeec

#birdeec.set_print_ir(True)
assert_generate_ok('''
{@from overload import *@}

@overloaded
func add[T1,T2](a as T1, b as T2)
end

@overloading("add")
func myadd(a as int, b as string)
	println(int2str(a)+b)
end

@overloading("add")
func myadd2(a as string, b as string)
	println(a+b)
end

@overloading("add")
func myadd3(a as string, b as float)
	println(a+double2str(b))
end

@overloading("add")
func myadd4(a as string, b as int)
	println(a+int2str(b))
end

@overloading_with("add", "boolean", "boolean")
func myaddbool(a as boolean, b as boolean)
	println("BOOOL")
end


{@
def match_checker(name):
	def run(expr):
		assert(expr.callee.funcdef.body[0].callee.funcdef.proto.name==name)
	return run
@}

@match_checker("myadd")
add(1,"asds")

@match_checker("myadd2")
add("ds", "sa")

@match_checker("myadd3")
add("ddd", 1.2)

@match_checker("myaddbool")
add(true, false)

#inperfect match
@match_checker("myadd")
add(1.2,"asds")

#select better match instead of inperfect one
@match_checker("myadd4")
add("asds", 1)

@overloaded
func addany[...](...)
end

@overloading("addany")
func addany1(a as string, b as string)
	println(a+b)
end
@match_checker("addany1")
addany("1", "2")
''')

#ambiguous overloading
assert_fail('''
{@from overload import *@}

@overloaded
func add[T1,T2](a as T1, b as T2)
end

@overloading("add")
func myadd1(a as int, b as string)
	println(int2str(a)+b)
end

@overloading("add")
func myadd2(a as double, b as string)
	println(int2str(a)+b)
end
add(1.23f, "sa")
''')

#no matching overload
assert_fail('''
{@from overload import *@}

@overloaded
func add[T1,T2](a as T1, b as T2)
end

@overloading("add")
func myadd(a as int, b as string)
	println(int2str(a)+b)
end
add("ds", "sa")
''')


#have a body
assert_fail('''
{@from overload import *@}

@overloaded
func add[T1,T2](a as T1, b as T2)
	println("HELLO")
end

@overloading("add")
func myadd(a as int, b as string)
	println(int2str(a)+b)
end
add(1, "sa")
''')

#not template
assert_fail('''
{@from overload import *@}
@overloaded
func add()
end
''')

#overloading a variable
assert_fail('''
{@from overload import *@}
dim aaa as int
@overloading("aaa")
func add()
end
''')
