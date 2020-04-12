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

def assert_generate_ok(istr):
	top_level(istr)
	process_top_level()
	generate()
	clear_compile_unit()

def assert_generate_fail(istr):
	try:
		top_level(istr)
		process_top_level()
		generate()
		assert(False)
	except TokenizerException:
		e=get_tokenizer_error()
		print(e.linenumber,e.pos,e.msg)
	except CompileException:
		e=get_compile_error()
		print(e.linenumber,e.pos,e.msg)		
	clear_compile_unit()