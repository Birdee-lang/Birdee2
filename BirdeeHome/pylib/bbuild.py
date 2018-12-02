import sys
import os
import json
import subprocess

#run %comspec% /k "D:\ProgramFiles\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat" first!!

source_dirs=[]
bin_search_dirs=[]
root_modules=[]
compiler_path=""
outpath=""
bd_home=''
link_path=[]
compiled_mod=set()
link_target=None
link_executable=False
runtime_lib_path=""



def get_next(idx,args):
	if idx+1>=len(args):
		raise RuntimeError("Expecting an argument after "+args[idx])
	return (idx+1,args[idx+1])
def parse_args(args):
	i=1
	global source_dirs,bin_search_dirs,root_modules,outpath,link_target,link_executable
	while i<len(args):
		if args[i]=='-i' or args[i]=='--in-source':
			i,v = get_next(i,args)
			source_dirs.append(v)
		elif args[i]=='-o' or args[i]=='--out':
			i,outpath = get_next(i,args)
		elif args[i]=='-bs' or args[i]=='--bin-search-path':
			i,v = get_next(i,args)
			bin_search_dirs.append(v)
		elif args[i]=='-le' or args[i]=='--link-executable':
			i,link_target = get_next(i,args)
			link_executable=True
		else:
			if(args[i][0]=='-'):
				raise RuntimeError("Unknown command "+args[i])
			root_modules.append(args[i].split('.'))
		i+=1
	if len(root_modules)==0 :
		raise RuntimeError("No root modules specified")
	if len(outpath)==0 :
		outpath='.'
	bin_search_dirs.append(os.path.join(bd_home,"blib"))
	if '.' not in source_dirs: source_dirs.append('.')
	if '.' not in bin_search_dirs: bin_search_dirs.append('.')

def search_bin(modu):
	global link_path
	for path in bin_search_dirs:
		raw_path=os.path.join(path,*modu)
		p = raw_path +".bmm"
		if os.path.exists(p) and os.path.isfile(p) :
			link_path.append(raw_path)
			return raw_path
	raw_path=os.path.join(outpath,*modu)
	p = raw_path +".bmm"
	if os.path.exists(p) and os.path.isfile(p) :
		#we have found the binary file, but if it need updating?
		src=search_src(modu)
		if src: #if it is in the source, check if source is newer than binary
			mtime_src=os.path.getmtime(src)
			mtime_bin=os.path.getmtime(p)
			if mtime_src > mtime_bin:
				return #act as if we do not find the binary file
		link_path.append(raw_path)
		return raw_path	

def search_src(modu):
	for path in source_dirs:
		raw_path=os.path.join(path,*modu)
		p = raw_path +".bdm"
		if os.path.exists(p) and os.path.isfile(p) :
			return p
		p = raw_path +".txt"
		if os.path.exists(p) and os.path.isfile(p) :
			return p

def parse_bmm_dependency(bmm_path):
	with open(bmm_path) as f:
		data = json.load(f)
		dependencies=data['Imports']
		for dep in dependencies:
			if dep[-1][0]==':':  #if it is a "name import"
				dep.pop() #pop the imported name
			compile_module(dep,False)

def create_dirs(root,modu):
	dirname=os.path.join(root,*modu[:-1])
	if not os.path.exists(dirname):
		os.makedirs(dirname)

def compile_module(modu,is_main):
	if tuple(modu) in compiled_mod:
		return
	bmm=search_bin(modu)
	if bmm:
		compiled_mod.add(tuple(modu))
		parse_bmm_dependency(bmm+".bmm")
	else:
		src=search_src(modu)
		if not src:
			raise RuntimeError("Cannot resolve module dependency: " + ".".join(modu))
		create_dirs(outpath,modu)
		outfile=os.path.join(outpath,*modu) 
		cmdarr=[compiler_path,'-i',src, "-o", outfile+'.obj']
		for bpath in bin_search_dirs:
			cmdarr.append("-l")
			cmdarr.append(bpath)
		if is_main:
			cmdarr.append("-e")
		cmd=" ".join(cmdarr)
		print("Running command " + cmd)
		ret=subprocess.run(cmd)
		if ret.returncode!=0:
			raise RuntimeError("Compile failed")
		compiled_mod.add(tuple(modu))
		link_path.append(outfile)
		parse_bmm_dependency(outfile+".bmm")

def init_path():
	global bd_home,compiler_path
	bd_home=os.environ.get('BIRDEE_HOME')
	if not bd_home:
		raise RuntimeError("The environment variable BIRDEE_HOME is not set")
	compiler_path=os.path.join(bd_home,"bin","birdeec.exe") # fix-me: exe???
	if not os.path.exists(compiler_path) or not os.path.isfile(compiler_path):
		raise RuntimeError("Cannot find birdee compiler")


init_path()
parse_args(sys.argv)
root_modules.append(['birdee'])
file_cnt=0
for modu in root_modules:
	compile_module(modu,file_cnt==0)
	file_cnt += 1
if link_executable and link_target:
	linker_path='link.exe'
	msvc_command='''{} /OUT:"{}" /MANIFEST /NXCOMPAT /PDB:"{}" /DYNAMICBASE {} "kernel32.lib" "user32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib" "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib" "odbc32.lib" "odbccp32.lib" /DEBUG /MACHINE:X64 /INCREMENTAL /SUBSYSTEM:CONSOLE /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /ManifestFile:"{}" /ERRORREPORT:PROMPT /NOLOGO /TLBID:1 '''
	runtime_lib_path = os.path.join(bd_home,"bin","BirdeeRuntime.lib")
	pdb_path= os.path.splitext(link_target)[0]+".pdb"
	obj_files=f'"{runtime_lib_path}"'
	for lpath in link_path:
		lpath += ".obj"
		obj_files += f' "{lpath}"'
	cmd=msvc_command.format(linker_path,link_target,pdb_path,obj_files,link_target+".manifest")
	print("Running command " + cmd)
	ret=subprocess.run(cmd)
	if ret.returncode!=0:
		raise RuntimeError("Compile failed")