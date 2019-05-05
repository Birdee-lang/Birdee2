import os
import os.path
import json

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

def get_code(name):
	template = "extern void {a}();\njit->addNative(\"{a}\",{a});\n\n"
	return template.format(a =name)


outf=open('BirdeeIncludes.h','w')
outf.write("extern \"C\"{\nvoid AddFunctions(llvm::orc::KaleidoscopeJIT* jit){\n")

def parse_bmm(*pkg):
	with open(os.path.join(os.environ['BIRDEE_HOME'],'blib',*pkg) + '.bmm') as f:
		data = json.load(f)
		pkgname = '.'.join(pkg) + '.'
		for func in data['Functions']:
			fstr=mangle_func(pkgname+func['name'])
			print(fstr)
			outf.write(get_code(fstr))
		for clazz in data['Classes']:
			if 'funcs' in clazz:
				for funcdef in clazz['funcs']:
					if 'def' in funcdef:
						func=funcdef['def']
						fstr=mangle_func(pkgname+clazz['name']+'.'+func['name'])
						print(fstr)
						outf.write(get_code(fstr))
		
	main_name = mangle_func(pkgname)+"_1main"
	outf.write(get_code(main_name))

parse_bmm('hash_map')
parse_bmm('concurrent','threading')
parse_bmm('string_buffer')
parse_bmm('birdee')
outf.write("}}\n")
print("done")