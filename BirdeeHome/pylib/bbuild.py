import sys
import os
import json
import subprocess
from queue import Queue
from threading import Thread
from threading import Lock
import locale
import traceback
#run %comspec% /k "D:\ProgramFiles\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat" first!!

source_dirs=[]
bin_search_dirs=[]
root_modules=[]
compiler_path=""
outpath=""
bd_home=''
link_path=[]
prepared_mod=dict()
link_target=None
link_executable=False
runtime_lib_path=""
max_bin_timestamp =0
num_worker_threads = 1
thread_worker=None
link_cmd = ""
exe_postfix = ""
obj_postfix = '.o'
if os.name == 'nt':
	exe_postfix = ".exe"
	obj_postfix = '.obj'

class compile_worker:
	RUNNING=0
	DONE=1
	ABORT=2
	ABORT_DONE=3
	def __init__(self):
		self.q = Queue()
		def worker():
			while True:
				cu = self.q.get()
				try:
					compile_module(cu.modu,cu.source_path,cu.is_main)
					for dep_cu in cu.reverse_dependency:
						if dep_cu.source_path and dep_cu.dependency_done():
							#the module dep_cu depends on cu and it is a source code module,
							#mark the current cu as done, and check if there are other dependencies
							self.put(dep_cu)
					cu.done=True
				except Exception as e:
					#print(traceback.format_exc())
					print("\n"+str(e))
					if num_worker_threads == 1:
						print(traceback.format_exc())
					self.state=compile_worker.ABORT
				finally:
					self.q.task_done()
		for i in range(num_worker_threads):
			 t = Thread(target=worker)
			 t.daemon = True
			 t.start()
		self.joiner=Thread(target=lambda:self.q.join())
		self.state=compile_worker.RUNNING
	
	def start_joiner(self):
		self.joiner.start()

	def join(self,timeout):
		self.joiner.join(timeout)
		if self.joiner.isAlive():
			if self.state==compile_worker.RUNNING:
				return compile_worker.RUNNING
			if self.state==compile_worker.ABORT:
				return compile_worker.ABORT
			else:
				raise RuntimeError("illegal state of compile_worker")
		else:
			if self.state==compile_worker.RUNNING:
				self.state=compile_worker.DONE
			if self.state==compile_worker.ABORT:
				self.state=compile_worker.ABORT_DONE
			return self.state
	
	def put(self,cu):
		if self.state!=compile_worker.ABORT and self.state!=compile_worker.ABORT_DONE:
			self.q.put(cu)

class compile_unit:
	def __init__(self,is_main,source_path,modu):
		self.is_main=is_main
		self.source_path=source_path
		self.modu=modu
		self.reverse_dependency=set()
		self.dependency_cnt = 0
		self.lock = Lock()
		self.done = source_path is None

	def add_reverse_dependency(self,cu):
		self.reverse_dependency.add(cu)

	#decrease dependency_cnt by 1, return true when this cu is ready to compile
	def dependency_done(self):
		self.lock.acquire()
		self.dependency_cnt-=1
		if self.dependency_cnt<0:
			self.lock.release()
			raise RuntimeError("Bad dependency_cnt")
		ret = self.dependency_cnt==0
		self.lock.release()
		return ret
			
	def add_dependency(self,cu):
		if cu.source_path:  #if the compile unit is a source file
			cu.add_reverse_dependency(self)
			self.dependency_cnt += 1
		#if cu is a binary module, we don't need to wait for it, so just ignore the dependency

def update_max_bin_timestamp(fn):
	global max_bin_timestamp
	ts=os.path.getmtime(fn)
	if ts>max_bin_timestamp:
		max_bin_timestamp=ts

def get_next(idx,args):
	if idx+1>=len(args):
		raise RuntimeError("Expecting an argument after "+args[idx])
	return (idx+1,args[idx+1])
def parse_args(args):
	i=1
	global source_dirs,bin_search_dirs,root_modules,outpath,link_target,link_executable,num_worker_threads,link_cmd
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
		elif args[i]=='-lc' or args[i]=='--link-cmd':
			i,link_cmd = get_next(i,args)
		elif args[i]=='-j':
			i,v = get_next(i,args)
			num_worker_threads = int(v)
			if num_worker_threads<1:
				raise RuntimeError("Bad number of threads")
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
	for path in bin_search_dirs:
		raw_path=os.path.join(path,*modu)
		p = raw_path +".bmm"
		if os.path.exists(p) and os.path.isfile(p) :
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

def parse_bmm_dependency(bmm_path,self_cu):
	with open(bmm_path) as f:
		data = json.load(f)
		dependencies=data['Imports']
		for dep in dependencies:
			if dep[-1][0]==':' or dep[-1][0]=='*':  #if it is a "name import"
				dep.pop() #pop the imported name
			self_cu.add_dependency(prepare_module(dep,False))

def create_dirs(root,modu):
	dirname=os.path.join(root,*modu[:-1])
	if not os.path.exists(dirname):
		os.makedirs(dirname)

def prepare_module(modu,is_main):
	tuple_modu=tuple(modu)
	if tuple_modu in prepared_mod:
		return prepared_mod[tuple_modu]
	bmm=search_bin(modu)
	cu=None
	if bmm:
		cu = compile_unit(False,None,tuple_modu)
		prepared_mod[tuple_modu] = cu
		update_max_bin_timestamp(bmm+".bmm")
		link_path.append(bmm)
		parse_bmm_dependency(bmm+".bmm", cu)
	else:
		src=search_src(modu)
		if not src:
			raise RuntimeError("Cannot resolve module dependency: " + ".".join(modu))
		cu = compile_unit(is_main,src,tuple_modu)
		prepared_mod[tuple_modu] = cu
		update_max_bin_timestamp(src)

		cmdarr=[compiler_path,'-i',src, "--print-import"]
		cmd=" ".join(cmdarr)
		print("Running command " + cmd)
		proc = subprocess.Popen(cmdarr,stdout=subprocess.PIPE)
		dependencies_list=[]
		while True:
			line = proc.stdout.readline().decode(locale.getpreferredencoding())
			if line != '':
				dependencies_list.append(line.rstrip())
			else:
				break
		dependencies_list.pop() #delete the last element, which is the package name of the source itself
		if proc.wait()!=0:
			raise RuntimeError("Compile failed, exit code: "+ str(proc.returncode))
		proc=None #hope to release resource for the process pipe
		for depstr in dependencies_list:
			dep=depstr.split(".")
			if dep[-1][0]==':' or dep[-1][0]=='*':  #if it is a "name import"
				dep.pop() #delete the imported name
			cu.add_dependency(prepare_module(dep,False))
		create_dirs(outpath,modu)
	return cu


def compile_module(modu,src,is_main):
	outfile=os.path.join(outpath,*modu) 
	cmdarr=[compiler_path,'-i',src, "-o", outfile+ obj_postfix]
	for bpath in bin_search_dirs:
		cmdarr.append("-l")
		cmdarr.append(bpath)
	cmdarr.append("-l")
	cmdarr.append(outpath)
	if is_main:
		cmdarr.append("-e")
	print("Running command " + " ".join(cmdarr))
	ret=subprocess.run(cmdarr)
	if ret.returncode!=0:
		raise RuntimeError("Compile failed")
	link_path.append(outfile)

def init_path():
	global bd_home,compiler_path
	bd_home=os.environ.get('BIRDEE_HOME')
	if not bd_home:
		raise RuntimeError("The environment variable BIRDEE_HOME is not set")
	compiler_path=os.path.join(bd_home,"bin","birdeec"+exe_postfix)
	if not os.path.exists(compiler_path) or not os.path.isfile(compiler_path):
		raise RuntimeError("Cannot find birdee compiler")

def link_msvc():
	linker_path='link.exe'
	msvc_command='''{} /OUT:"{}" /MANIFEST /NXCOMPAT /PDB:"{}" {} /DYNAMICBASE {} "kernel32.lib" "user32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib" "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib" "odbc32.lib" "odbccp32.lib" /DEBUG /MACHINE:X64 /INCREMENTAL /SUBSYSTEM:CONSOLE /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /ManifestFile:"{}" /ERRORREPORT:PROMPT /NOLOGO /TLBID:1 '''
	runtime_lib_path = os.path.join(bd_home,"bin","BirdeeRuntime.lib")
	pdb_path= os.path.splitext(link_target)[0]+".pdb"
	obj_files='"{runtime_lib_path}"'.format(runtime_lib_path=runtime_lib_path)
	for lpath in link_path:
		lpath += obj_postfix
		obj_files += ' "{lpath}"'.format(lpath=lpath)
	cmd=msvc_command.format(linker_path,link_target,pdb_path,link_cmd,obj_files,link_target+".manifest")
	print("Running command " + cmd)
	ret=subprocess.run(cmd)
	if ret.returncode!=0:
		raise RuntimeError("Compile failed")


def link_gcc():
	linker_path='gcc'
	cmdarr = [linker_path,'-o',link_target, "-Wl,--start-group"]
	runtime_lib_path = os.path.join(bd_home,"lib","libBirdeeRuntime.a")
	cmdarr.append(runtime_lib_path)
	for lpath in link_path:
		lpath += obj_postfix
		cmdarr.append(lpath)
	cmdarr.append("-lgc")
	cmdarr.append("-Wl,--end-group")
	if len(link_cmd):
		cmdarr.append(link_cmd)
	print("Running command " + ' '.join(cmdarr))
	ret=subprocess.run(cmdarr)
	if ret.returncode!=0:
		raise RuntimeError("Compile failed")


init_path()
parse_args(sys.argv)
root_modules.append(['birdee'])
file_cnt=0
for modu in root_modules:
	prepare_module(modu,file_cnt==0)
	file_cnt += 1

thread_worker=compile_worker()
for modu,cu in prepared_mod.items():
	if cu.source_path and cu.dependency_cnt==0: #if a module is waiting for compiling and all dependencies are resolved
		thread_worker.put(cu)
thread_worker.start_joiner()
while True:
	status = thread_worker.join(0.1)
	if status == compile_worker.ABORT:
		print("Aborted, waiting for tasks completion")
		while thread_worker.join(0.1)!=compile_worker.ABORT_DONE: pass
		break
	if status == compile_worker.ABORT_DONE:
		print("Aborted")
		break
	if status == compile_worker.DONE:
		break
		


for modu,cu in prepared_mod.items():
	if not cu.done:
		raise RuntimeError("The compile unit " + ".".join(cu.modu) + " is not compiled")

if link_executable and link_target:
	if os.path.exists(link_target) and os.path.isfile(link_target) and  os.path.getmtime(link_target)>max_bin_timestamp:
		print("The link target is up to date")
	else:
		if os.name=='nt':
			link_msvc()
		else:
			link_gcc()


