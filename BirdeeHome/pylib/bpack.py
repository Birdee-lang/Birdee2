import bpack_utils.resolver as resolver
import argparse
import os
import json
import bbuild


def do_init(name: str, author:str):
    if not os.path.exists("bpackproject.json"):
        data = {'name':name, 'version': '0.0.1', "author":author, "dependencies":[]}
        with open("bpackproject.json", 'w') as f:
            json.dump(data, f, indent=4, sort_keys=True)
    os.makedirs("bin", exist_ok=True)
    os.makedirs("src", exist_ok=True)

def load_meta():
    if not os.path.exists("bpackproject.json"):
        print("Cannot find 'bpackproject.json'. Have you initalize the project by 'bpack.py init'?")
        exit(-1)
    with open("bpackproject.json") as f:
        return json.load(f)    

def write_meta(meta):
    with open("bpackproject.json", 'w') as f:
            json.dump(meta, f, indent=4, sort_keys=True)

def do_resolve(meta):
    roots, paths = resolver.download_dependencies(meta['dependencies'])
    if len(roots)!=len(meta['dependencies']):
        meta['dependencies']=roots
        write_meta(meta)
    return paths

parser = argparse.ArgumentParser()
subparsers = parser.add_subparsers(help='The main command', dest="command")

init_parser = subparsers.add_parser("init", help='Initalize the current directory as a Birdee Project directory')
init_parser.add_argument("--name", type=str, dest= "name", required=True, help="The name of the project")
init_parser.add_argument("--author", type=str, dest= "author", required=True, help="The author of the project")

resolve_parser = subparsers.add_parser("resolve", help='Resolve and download the dependencies of the project')

init_parser = subparsers.add_parser("build", help='Compile the project')

args=parser.parse_args()

if args.command=='init':
    do_init(args.name, args.author)
elif args.command=='resolve':
    meta=load_meta()
    do_resolve(meta)
elif args.command=='build':
    meta=load_meta()
    if 'root_modules' not in meta or len(meta['root_modules'])==0:
        print("You should specify 'root_modules' in bpackproject.json")
        exit(-1)
    paths = ["src"]
    paths.extend(do_resolve(meta))
    ctx=bbuild.context()
    #paths.append(os.path.join(bbuild.bd_home,"blib"))
    ctx.bin_search_dirs.append(os.path.join(bbuild.bd_home,"blib"))
    ctx.source_dirs=paths
    ctx.root_modules=[itm.split('.') for itm in meta['root_modules']]
    ctx.outpath="bin"
    if 'link_target' in meta:
        if meta['link_target'] == 'executable':
            ctx.link_executable = bbuild.LINK_EXECUTEBALE
            ctx.link_target = os.path.join("bin", meta['root_modules'][0]+bbuild.exe_postfix)
        elif meta['link_target'] == 'dynlib':
            ctx.link_executable = bbuild.LINK_SHARED
            ctx.link_target = os.path.join("bin", meta['root_modules'][0]+bbuild.dll_postfix)           
        else:
            print("Unknown link target " + meta['link_target'])
            exit(-1)
    #self.link_target=None
    #self.link_executable=None
    #self.num_worker_threads=1
    #self.link_cmd = ""
    bbuild.build(ctx)