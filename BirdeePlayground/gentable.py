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


obj_files=[]
outf=open('BirdeeIncludes.h','w')
outf.write("extern \"C\"{\nvoid AddFunctions(llvm::orc::KaleidoscopeJIT* jit){\n")


def add_lib_pragma(name):
	template = "#pragma comment(lib, \"{a}\")\n" 
	obj_files.append(template.format(a = name).replace("\\","\\\\"))

def write_lib_pargma():
	for line in obj_files:
		outf.write(line)

def parse_bmm(*pkg):
	add_lib_pragma(os.path.join(os.environ['BIRDEE_HOME'],'blib',*pkg) + '.obj')
	with open(os.path.join(os.environ['BIRDEE_HOME'],'blib',*pkg) + '.bmm') as f:
		data = json.load(f)
		pkgname = '.'.join(pkg) + '.'
		for var in data['Variables']:
			fstr=""
			fstr=mangle_func(pkgname+var['name'])
			print(fstr)
			outf.write(get_code(fstr))
		for func in data['Functions']:
			fstr=""
			if 'link_name' in func:
				continue
			else:
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

parse_bmm('extensions','string')
parse_bmm('hash')
parse_bmm('concurrent','threading')
parse_bmm('concurrent','sync')
parse_bmm('concurrent','syncdef')
parse_bmm('string_buffer')
parse_bmm('system','io','file')
parse_bmm('system','io','stream')
parse_bmm('system','io','filedef')
parse_bmm('system','io','stdio')
parse_bmm('system','specific','win32','file')
parse_bmm('system','specific','win32','concurrent')
parse_bmm('birdee')
outf.write("}}\n")
#write_lib_pargma()
print("done")