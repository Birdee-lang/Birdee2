import urllib.request
import os
import sys
import os.path as ospath
from urllib.request import urlretrieve
import json
import zipfile
import shutil

if 'BIRDEE_HOME' not in os.environ:
    raise RuntimeError("Cannot find BIRDEE_HOME in environment variables")

birdee_home_path = os.environ['BIRDEE_HOME']
sysbits = '64' if sys.maxsize > 2**32 else '32'
osname="unknown"
if sys.platform == 'linux2':
    osname='linux' + sysbits
elif sys.platform == 'win32':
    osname='win' + sysbits

class DownloaderInfo:
    def __init__(self, meta, package):
        self.meta=meta
        self.package=package

templates = {
    'github': DownloaderInfo("https://raw.githubusercontent.com/{author}/{name}/{version}/bpackproject.json", "https://github.com/{author}/{name}/archive/{version}.zip"),
    'local': DownloaderInfo("http://localhost:8000/{author}/{name}/{version}/bpackproject.json", "http://localhost:8000/{author}/{name}/{version}.zip")
    }

def download_file(url, file_name):
    if not url.startswith('http://localhost'):
        proxy_support = urllib.request.ProxyHandler({'http': 'http://localhost:1080',
            'https': 'https://localhost:1080'
            })
        opener = urllib.request.build_opener(proxy_support)
        urllib.request.install_opener(opener)
    print("Downloading ", url)
    def reporthook(blocknum, blocksize, totalsize):
        readsofar = blocknum * blocksize
        if totalsize > 0:
            percent = min(100., readsofar * 1e2 / totalsize)
            s = "\r%5.1f%% %*d / %d" % (
                percent, len(str(totalsize)), readsofar, totalsize)
            sys.stderr.write(s)
            if readsofar >= totalsize: # near the end
                sys.stderr.write("\n")
        else: # total size is unknown
            sys.stderr.write("read %d\n" % (readsofar,))

    urlretrieve(url, file_name, reporthook)

def check_if_author_valid(author):
    if '.' not in author or author[-1]=='.' or author[0]=='.':
        raise RuntimeError("There must be a . in the author name and . must not be the last or the first character. Got " + author)

def parse_package_name(pac: str):
    '''
    returns the author, lib name and version
    '''
    spl = pac.split(':')
    if len(spl)!=3:
        raise RuntimeError("There must be two ':' in the package, but got " + pac)
    check_if_author_valid(spl[0])
    if len(spl[1])==0 or len(spl[2])==0:
        raise RuntimeError("Empty package or version. Got "+ pac)
    return spl[0], spl[1], spl[2]

class ModuleDownloader:
    def __init__(self, author: str, name: str, version: str):
        self.author = author
        self.name = name
        self.version = version
        self.local_path = ospath.join(birdee_home_path, "installed" ,author, name, version)
        check_if_author_valid(author)
        fnd = author.find('.')
        self.domain = author[:fnd]
        self.username = author[fnd+1:]
        if self.domain not in templates:
            raise RuntimeError("Cannot recognize the domain: " + self.domain)
        self.templ: DownloaderInfo = templates[self.domain]
        self.metafile_path = os.path.join(self.local_path, "bpackproject.json")
        self.required_by = set()
        self.isseleted=False
        self.meta=None
    
    def download_metadata(self):
        download_file(self.templ.meta.format(author=self.username, name=self.name, version=self.version),
            self.metafile_path + ".cache")

    def download_source(self):
        if os.path.exists(self.metafile_path):
            #print("Use cached source for", str(self))
            return self.local_path
        pkgzip_path=os.path.join(self.local_path, "source.zip")
        download_file(self.templ.package.format(author=self.username, name=self.name, version=self.version),
            pkgzip_path)
        with zipfile.ZipFile(pkgzip_path,"r") as zip_ref:
            unzipedpath=os.path.join(self.local_path, "unziped")
            os.makedirs(unzipedpath, exist_ok=True)
            zip_ref.extractall(unzipedpath)
            zipdir=os.path.join(unzipedpath,zip_ref.namelist()[0])
            files = os.listdir(zipdir)
            for f in files:
                shutil.move(os.path.join(zipdir, f), self.local_path)
            shutil.rmtree(unzipedpath)
        os.remove(pkgzip_path)
        download_extra_binary(self.meta, self.local_path)
        return self.local_path

    def get_linker_args(self,out):  
        libpath=os.path.join(self.local_path,"dep", osname)
        collect_linker_args(self.meta, libpath, out)

    def __str__(self):
        return "{}:{}:{}".format(self.author,self.name,self.version)

    def add_reverse_dependency(self, dep):
        if dep:
            self.required_by.add(dep)

def collect_linker_args(meta, deppath, out):
    if 'linker_args' in meta:
        if osname in meta['linker_args']:
            for itm in meta['linker_args'][osname]:
                out.add(itm.format(LIBPATH=deppath))

def download_extra_binary(meta, outpath):
    if 'extra' in meta:
        if osname in meta['extra']:
            url= meta['extra'][osname]
            unzipedpath=os.path.join(outpath, "dep", osname)
            if os.path.exists(unzipedpath):
                return
            zippath=os.path.join(outpath, "__extra_tmp.zip")
            download_file(url, zippath)
            with zipfile.ZipFile(zippath,"r") as zip_ref:
                os.makedirs(unzipedpath, exist_ok=True)
                zip_ref.extractall(unzipedpath)
            os.remove(zippath)

def module_garbage_collect(all_dependencies: dict):
    need_run=True
    all_mods = set(all_dependencies.values()) #the index of all modules
    while need_run:
        need_run=False
        for itr in list(all_dependencies.values()):
            m: ModuleDownloader=itr
            for req in list(m.required_by):
                if isinstance(req, ModuleDownloader):
                    if not req in all_mods: #if the reverse dependency is not in the selected modules
                        m.required_by.remove(req)
            if len(m.required_by)==0: #no reverse dependencies
                need_run=True
                #remove the module
                all_dependencies.__delitem__((m.author, m.name))
                all_mods.remove(m)
                print("Removing package", m)

def _resolve_library_meta(author, name, version, required_by: ModuleDownloader, preselected: dict, all_dependencies, user_selections: set):
    if (author, name) in preselected: #if the module is in the root dependency, only use the selected version
        if version != preselected[(author, name)]:
            return
    m = ModuleDownloader(author, name, version)
    need_gc = False
    if (author, name) in all_dependencies:
        #if the package is already imported
        oldmod: ModuleDownloader = all_dependencies[(author, name)]
        if oldmod.version != version:
            if oldmod.isseleted:
                print("Selecting {B} instead of {A}".format(A=m, B=oldmod))
                oldmod.add_reverse_dependency(required_by)
                return
            else:
                print('''There is a version conflict between {A} (required by {ADEP}) and {B} (required by {BDEP})
    Please select a dependency:
    0. Abort
    1. {A}
    2. {B}
    Enter 0, 1 or 2'''.format(A=m, ADEP=required_by, B=oldmod, BDEP='.'.join([str(mod) for mod in oldmod.required_by])))
                imp = input()
                if imp=='2':
                    oldmod.isseleted=True
                    oldmod.add_reverse_dependency(required_by)
                    user_selections.add(str(oldmod))
                    return
                elif imp=='1':
                    m.isseleted=True
                    need_gc=True
                    for itm in oldmod.required_by:
                        m.add_reverse_dependency(itm)
                    user_selections.add(str(m))
                    #need to import the current package, pass through
                else:
                    print("Aborting")
                    raise RuntimeError("Version conflict")
        else:
            oldmod.add_reverse_dependency(required_by)
            return
            
    m.add_reverse_dependency(required_by)
    all_dependencies[(author, name)] = m
    if need_gc:
        module_garbage_collect(all_dependencies)
    if not ospath.exists(m.metafile_path+ ".cache"):
        os.makedirs(m.local_path, exist_ok=True)
        m.download_metadata()
    with open(m.metafile_path+ ".cache") as f:
        meta = json.load(f)
    m.meta=meta
    if 'dependencies' in meta:
        #first make a shadowed "preselected" dict. Add the package's own preselected into the shadowed one
        shadowed_preselected = preselected.copy()
        for itm in meta['dependencies']:
            subauthor, subname, subversion = parse_package_name(itm)
            if (subauthor, subname) in shadowed_preselected:
                if shadowed_preselected[(subauthor, subname)]!=subversion:
                    print("Selecting {B} instead of {A}".format(A=itm,
                        B="{}:{}:{}".format(subauthor, subname, shadowed_preselected[(subauthor, subname)])))
            else:
                shadowed_preselected[(subauthor, subname)] = subversion
        #recursively pass the "preselected" dict to the dependencies
        for itm in meta['dependencies']:
            ret = parse_package_name(itm)
            subauthor, subname, subversion = ret
            _resolve_library_meta(subauthor, subname, subversion, m, shadowed_preselected, all_dependencies, user_selections)

def download_dependencies(pkgs):
    all_dependencies=dict()
    preselected = dict() #the selected versions by the root dependencies
    user_selections=set()
    for pkg in pkgs:
        subauthor, subname, subversion = parse_package_name(pkg)
        preselected[subauthor, subname]=subversion
    for pkg in pkgs:
        subauthor, subname, subversion = parse_package_name(pkg)
        _resolve_library_meta(subauthor, subname, subversion, "root", preselected, all_dependencies, user_selections)
    names = ', '.join([ m.author + ":" + m.name + ":" + m.version for m in all_dependencies.values() ])
    print("The packages to be installed:", names)
    paths=[]
    linker_args=set()
    for m in all_dependencies.values():
        paths.append(m.download_source())
        m.get_linker_args(linker_args)
    retpkgs=pkgs[:]
    retpkgs.extend(list(user_selections))
    return retpkgs, paths, linker_args

#print(download_dependencies(["local.Menooker:Birdee2:1.0.0"])) #,"local.Menooker:bdvm:master"