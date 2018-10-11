from birdeec import *

top_level(
'''
{@from getset import *@}

dim c as string[]

@create_getter
@create_setter
class SomeClass
	private a as int
	private b as string
	private c as string[]
end

dim f as SomeClass=new SomeClass
f.set_c(new string * 3)
println(f.get_c()[0])
'''
)

process_top_level()