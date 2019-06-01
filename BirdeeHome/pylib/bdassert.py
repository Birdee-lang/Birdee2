from bdutils import *

def bdassert(expr):
	strpos = str(get_cur_script().pos)
	set_stmt('''if true then
	dim _v = {expr}
	if ! _v then
		throw new runtime_exception(\'\'\'Assertion failed! Position: {strpos}\'\'\')
	end
end
'''.format(expr=expr,strpos=strpos))