def mangle_func(name):
	out=''
	for s in name:
		if s == '_':
			out += '__'
		elif s == '.':
			out += '_0'
		elif s == '[':
			out += '_2'
		elif s == ']':
			out += '_3'
		elif s == ',':
			out += '_4'
		elif s == ' ':
			out += '_5'
		elif s.isalnum(): #is number of [a-z][A-Z]
			out += s
		else:
			out += '_x%0.2X' % ord(s)
	return out