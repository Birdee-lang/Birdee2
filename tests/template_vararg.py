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
function add[T1,T2](a as T2, b as T2) as T1
	return a+b
end

add[float](1,2)
''')


assert_fail('''
function add[T1,T2,T3](a as T2, b as T2) as T1
	return a+b
end

add[float](1,2)
''')

assert_ok('''
class cls[T,T2,...VArg]
	public a as T
	public function f[...FArg](...)
	end
	public function f2[...FArg](...VArg)
	end
end
new cls[int,int,boolean].f[string]("123")
new cls[int,int,boolean].f2[string](true)
''')


assert_ok('''
dim tuple1 as tuple[int,int]

{@
def tuple_add_fields(cls):
	assert(isinstance(cls,ClassAST))
	idx=1
	for arg in cls.template_instance_args:
		if arg.kind!=TemplateArgument.TemplateArgumentType.TEMPLATE_ARG_TYPE:
			raise RuntimeError("The {}-th template argument should be a type".format(idx))
		ty = arg.resolved_type
		class_body(cls, f'public v{idx} as {ty}\\n')
		idx+=1
@}

@tuple_add_fields
class tuple[...]
	public function __init__(...)

	end
end

dim t as tuple[int,float]=new tuple[int,float](2,3.4)
dim v1 = t.v1
dim v2 = t.v2
''')

assert_fail('''
class cls[T,T2,...VArg]
	public a as T

end
dim p2 as cls[int]
''')


assert_fail('''
function add2(a as int, ...) as int
	return a
end
''')

assert_fail('''
function add3[...](...Hello) as int
	return 0
end
add3[int]()
''')


assert_ok('''
func addn[...](...) as int
	return {@
arr_args=[]
for arg in get_cur_func().proto.args:
	arr_args.append(arg.name)
set_ast(expr(" + ".join(arr_args)))
@}
end

addn(1,2,3,4)
addn(1,2)
''')