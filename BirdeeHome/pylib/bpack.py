import bpack_utils.resolver as resolver
import argparse
import os
import json
import bbuild
import sys

curdir = os.getcwd()

def do_init(name: str, author:str):
    metafile=os.path.join(curdir, "bpackproject.json")
    if not os.path.exists(metafile):
        data = {'name':name, 'version': '0.0.1', "author":author, "dependencies":[]}
        with open(metafile, 'w') as f:
            json.dump(data, f, indent=4, sort_keys=True)
    os.makedirs(os.path.join(curdir,"bin"), exist_ok=True)
    os.makedirs(os.path.join(curdir,"src"), exist_ok=True)
    gitignorefile=os.path.join(curdir, ".gitignore")
    if not os.path.exists(gitignorefile):
        with open(gitignorefile, 'w') as f:
            f.write('''bin/*
dep/*
.BirdeeCache/*
''')

def load_meta():
    metafile=os.path.join(curdir, "bpackproject.json")
    if not os.path.exists(metafile):
        print("Cannot find 'bpackproject.json'. Have you initalize the project by 'bpack.py init'?")
        exit(-1)
    with open(metafile) as f:
        return json.load(f)    

def write_meta(meta):
    metafile=os.path.join(curdir, "bpackproject.json")
    with open(metafile, 'w') as f:
        json.dump(meta, f, indent=4, sort_keys=True)

def do_resolve(meta):
    resolver.download_extra_binary(meta, curdir)
    roots, paths, linker_args = resolver.download_dependencies(meta['dependencies'])
    if len(roots)!=len(meta['dependencies']):
        meta['dependencies']=roots
        write_meta(meta)
    return paths, linker_args

parser = argparse.ArgumentParser()
subparsers = parser.add_subparsers(help='The main command', dest="command")

init_parser = subparsers.add_parser("init", help='Initalize the current directory as a Birdee Project directory')
init_parser.add_argument("--name", type=str, dest= "name", required=True, help="The name of the project")
init_parser.add_argument("--author", type=str, dest= "author", required=True, help="The author of the project")

resolve_parser = subparsers.add_parser("resolve", help='Resolve and download the dependencies of the project')

init_parser = subparsers.add_parser("build", help='Compile the project')

init_parser = subparsers.add_parser("download", help='Download a library')
init_parser.add_argument('libnames', metavar='PACKAGE_NAME', type=str, nargs='+',
                    help='The library names to install')

args=parser.parse_args()

if args.command=='init':
    do_init(args.name, args.author)
elif args.command=='resolve':
    meta=load_meta()
    do_resolve(meta)
elif args.command=='download':
    resolver.download_dependencies(args.libnames)
elif args.command=='build':
    meta=load_meta()
    if 'root_modules' not in meta or len(meta['root_modules'])==0:
        print("You should specify 'root_modules' in bpackproject.json")
        exit(-1)
    paths = [os.path.join(curdir, "src")]
    _paths, linker_args = do_resolve(meta)
    resolver.collect_linker_args(meta, os.path.join(curdir, "dep", resolver.osname), linker_args)
    paths.extend(_paths)
    ctx=bbuild.context()
    #paths.append(os.path.join(bbuild.bd_home,"blib"))
    ctx.bin_search_dirs.append(os.path.join(bbuild.bd_home,"blib"))
    ctx.source_dirs=paths
    ctx.root_modules=[itm.split('.') for itm in meta['root_modules']]
    ctx.outpath=os.path.join(curdir, "bin")
    ctx.link_cmd = ' '.join(linker_args)
    if 'link_target' in meta:
        if meta['link_target'] == 'executable':
            ctx.link_executable = bbuild.LINK_EXECUTEBALE
            ctx.link_target = os.path.join(curdir, "bin", meta['root_modules'][0]+bbuild.exe_postfix)
        elif meta['link_target'] == 'dynlib':
            ctx.link_executable = bbuild.LINK_SHARED
            ctx.link_target = os.path.join(curdir, "bin", meta['root_modules'][0]+bbuild.dll_postfix)           
        else:
            print("Unknown link target " + meta['link_target'])
            exit(-1)
    bbuild.build(ctx)