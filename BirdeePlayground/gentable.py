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


f=open(os.path.join(os.environ['BIRDEE_HOME'],'blib','birdee.bmm'))
data = json.load(f)
outf=open('BirdeeIncludes.h','w')
outf.write("extern \"C\"{\nvoid AddFunctions(llvm::orc::KaleidoscopeJIT* jit){\n")
for func in data['Functions']:
	fstr=mangle_func("birdee."+func['name'])
	print(fstr)
	outf.write(get_code(fstr))
for clazz in data['Classes']:
		for funcdef in clazz['funcs']:
			func=funcdef['def']
			fstr=mangle_func("birdee."+clazz['name']+'.'+func['name'])
			print(fstr)
			outf.write(get_code(fstr))
print("birdee_0_1main")
outf.write(get_code("birdee_0_1main"))
outf.write("}}\n")
print("done")