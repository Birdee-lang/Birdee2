
# Building Birdee on Ubuntu

Birdee compiler depends on: LLVM (version 6, newer are not tested), g++ (any version that supports -std=c++14), pybind11, bdwgc, libunwind (already installed with gcc) and [nlohmann's JSON library](https://github.com/nlohmann/json). First, you need to update your g++ to make it support C++14 and update git (>=2.0).

Then, install LLVM. You may refer to [LLVM's apt site](https://apt.llvm.org/) for instructions. Here we provide commands for installing LLVM on Ubuntu 14.04. You need first add the following lines to /etc/apt/sources.list:

```bash
deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-6.0 main
deb-src http://apt.llvm.org/trusty/ llvm-toolchain-trusty-6.0 main
deb http://ppa.launchpad.net/ubuntu-toolchain-r/test/ubuntu trusty main
```

Run the following command:

```bash
wget -O - http://apt.llvm.org/llvm-snapshot.gpg.key|sudo apt-key add -
sudo apt-get update
sudo apt-get install llvm-6.0
sudo apt-get install python3-dev
sudo apt-get install libgc-dev
pip3 install pybind11
sudo ln -s -r /usr/include/llvm-6.0/llvm /usr/include/llvm
sudo ln -s -r /usr/include/llvm-c-6.0/llvm-c/ /usr/include/llvm-c
```

Then fetch Birdee's source code and download other dependencies:
```bash
git clone https://github.com/Menooker/Birdee2
cd Birdee2
mkdir dependency
cd dependency
git init
git remote add -f origin https://github.com/pybind/pybind11
git checkout origin/master include/pybind11
mkdir -p include/nlohmann
cd include/nlohmann
wget https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
wget https://raw.githubusercontent.com/nlohmann/fifo_map/master/src/fifo_map.hpp
cd ../../../
```

One more step, set some required environment variables. You can add something like (assuming that the root directory of Birdee source code is /home/menooker/Birdee2):
```bash
export BIRDEE_HOME=/home/menooker/Birdee2/BirdeeHome/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$BIRDEE_HOME/lib
```

to your `/etc/profile` or `~/.bashrc`.

Finally, make Birdee!

```bash
make -j4
```

# Building Birdee on with Visual Studio

This repo includes a Visual Studio 2017 solution. Clone this repo and open "Birdee.sln", then you can view the source code of Birdee. However, to compile Birdee, a few more steps needs to be done.

## Step 1 for VS (Install LLVM)

You can either statically link with LLVM or link with LLVM DLL. Linking with static LLVM library makes the linking time of the compiler itself (BirdeeCompilerCore) much longer and takes more time to build Birdee compiler. However a statically linked Birdee compiler can be distributed without the DLL of LLVM. On the other hand, dynamically linked version of LLVM makes Birdee compiler faster to build and results in smaller executable files. To build Birdee with statically linked LLVM, use the "Debug" configuration of the Visual Studio Solution. To build with LLVM DLL, please switch to the "DynLLVM" configuration.

Assume that the root directory of Birdee source code is "Birdee". Then create directory "Birdee\\dependency" and "Birdee\\dependency\\bin".

### Either you can build/install static LLVM library
First, you need a copy of LLVM 6.0 (or maybe newer). You can compile LLVM by yourself with CMake, or you can download a pre-compiled LLVM binary built by me.

 * LLVM-Windows-x64-Debug (Debug version with symbols) [BaiduYun](https://pan.baidu.com/s/1Yb4GPKIuYlQcXcKWd7tXRA) [GoogleDrive](https://drive.google.com/open?id=1Jeh8Dm9ca7u119yvsytv_SaHQNqHaq1m)
 * LLVM-Windows-x64-Release (Release version) [BaiduYun](https://pan.baidu.com/s/1JJPzMSNf9XRaSzzHO42DiA) [GoogleDrive](https://drive.google.com/open?id=1TKdx8wxkdvz1Mx2Fzpluc7-XVEa2gaeH)
 * Headers for Windows x64 [BaiduYun](https://pan.baidu.com/s/1kOfgfwvV37VHNa5vwqHciw) [GoogleDrive](https://drive.google.com/open?id=1UONnbLtPzAftrAks9Vhdkb8iDC4rmdqA)

If you have compiled LLVM by yourself,

 * link/copy "path_to_llvm_src/llvm-6.0.0.src/include" to  "Birdee\\dependency\\llvm-include"
 * link/copy "path_to_llvm_cmake_project/include" to "Birdee\\dependency\\llvm-build-include"
 * link/copy "path_to_llvm_cmake_project/Debug/lib" to "Birdee\\dependency\\bin\\llvm-debug" 
 * link/copy "path_to_llvm_cmake_project/Release/lib" to "Birdee\\dependency\\bin\\llvm-release" 

If you have downloaded pre-compiled LLVM,

 * extract "llvm.6.0.win.include.zip/llvm-include" to  "Birdee\\dependency\\llvm-include"
 * extract "llvm.6.0.win.include.zip/llvm-build-include" to "Birdee\\dependency\\llvm-build-include"
 * extract "llvm.6.0.win.Debug.zip/lib" to "Birdee\\dependency\\bin\\llvm-debug" 
 * (Not currently needed, you can skip this step) extract "llvm.6.0.win.Release.zip/lib" to "Birdee\\dependency\\bin\\llvm-release" 

### Or you can build/install dynamic LLVM library

First download LLVM dynamic library and headers for Windows built by me.

 * LLVM-shared-build [BaiduYun, share code: jq4r](https://pan.baidu.com/s/1XVp3FPUQ_kCXpeFLRicN6g) [GoogleDrive](https://drive.google.com/open?id=1dnWGEZI1VUnZrLRCpcXCD7Uvo6QTob2A)
 * Headers for Windows x64 (The same as in the above section) [BaiduYun](https://pan.baidu.com/s/1kOfgfwvV37VHNa5vwqHciw) [GoogleDrive](https://drive.google.com/open?id=1UONnbLtPzAftrAks9Vhdkb8iDC4rmdqA)

Then,

 * extract "llvm.6.0.win.include.zip/llvm-include" to  "Birdee\\dependency\\llvm-include"
 * extract "llvm.6.0.win.include.zip/llvm-build-include" to "Birdee\\dependency\\llvm-build-include"
 * extract "LLVM-shared-build.zip/\*" to "Birdee\\dependency\\bin\\llvm-debug-dyn"
 * Link/copy Birdee\\dependency\\bin\\llvm-debug-dyn\\LLVM-6.0.dll to Birdee\\x64\\DynLLVM\\LLVM-6.0.dll (You may need to create the directories)

The DLL file "LLVM-6.0.dll" is built from "Debug" version of LLVM, and has been tailored for the use of Birdee Compiler. It only include needed ".lib" files from LLVM and only exports needed symbols of Birdee. For reference, I built it in the following steps:

 * Build LLVM static libraries
 * Run compilation of Birdee compiler, MSVC will complain on missing symbols.
 * Parse the MSVC linker error message, a list of needed symbols from LLVM is generated
 * Convert the list of symbols to a ".DEF" file to generate DLL
 * Re-link the LLVM static libraries to generate a single DLL using `link /DLL /DEF LLVM-6.0.def ...`

I have done the above steps for you in the file `LLVM-shared-build.zip`. You can view the txt files in the zip for more details.

## Step 2 for VS (Install pybind11)
Then make sure you have installed an x64 version of Python. Copy/Link "python3.lib" and "python3X.lib" from Python to "Birdee\\dependency\\bin" ("X" in the file name is the exact subversion of your Python). These two files are located in "lib" directory of Python's installed directory.

Run command

```cmd
pip install pybind11
```

Make sure the "pip" program is provided exactly by the same version of Python to be used by Birdee. Finally, link/copy the "include" directory from Python's installed directory to "Birdee\\dependency\\pyinclude"

## Step 3 for VS (Install header-only dependencies)

Create a directory "Birdee\\dependency\\include" and "Birdee\\dependency\\include\\nlohmann". Download [header](https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp) and [header](https://raw.githubusercontent.com/nlohmann/fifo_map/master/src/fifo_map.hpp) to "Birdee\\dependency\\include\\nlohmann".

## Step 4 for VS (Install bdwgc and libunwind from libgcc)

bdwgc is a library for conservative GC, which is used for Birdee's runtime. Like LLVM, you can either download a pre-built binary ([BaiduYun](https://pan.baidu.com/s/1T39OUmZBZcw5T5I9LEuiyg),[GoogleDrive](https://drive.google.com/open?id=1wA9ctJvcopfGAYxZVfEhacBzY9APw_S6)), or build the library by yourself. Here is some notes for building bdwgc.

 * Make sure you have Visual Studio installed. Open Visual Studio x64 Commmand Prompt. (You can find it in the Visual Studio directory of the start menu.)
 * Download and extract sources of [bdwgc](https://github.com/ivmai/bdwgc/releases) and [libatomic_ops](https://github.com/ivmai/libatomic_ops/releases). 
 * In Visual Studio x64 Commmand Prompt, switch directory to "XXXXX/libatomic_ops-7.6.6/src", and run "nmake -f Makefile.msft"
 * Copy "atomic_ops.h","atomic_ops","atomic_ops_stack.h","atomic_ops_malloc.h" and "libatomic_ops_gpl.lib" in "src" directory to the directory "XXXXX/gc-7.6.8/include" (source code for bdwgc)
 * Then switch directory to "XXXXX/gc-7.6.8" (source code for bdwgc). Run "nmake -f NT_MAKEFILE cpu=AMD64 nodebug=1"

The above sources and generated files are included in the pre-built binary. Then:
 * Link/copy XXXXX\gc-7.6.8\gc64_dll.lib to Birdee\dependency\bin\gc64_dll.lib
 * Link/copy XXXXX\gc-7.6.8\include to Birdee\dependency\gc_include
 * Link/copy XXXXX\gc-7.6.8\gc64.dll to Birdee\x64\debug\gc64.dll

Then install libunwind which is part of libgcc. It is available at [BaiduYun](https://pan.baidu.com/s/1vfMaAmNmDphXml41qkygWA) or [GoogleDrive](https://drive.google.com/open?id=1vK1Y4cR4UlS035T8EJ6YAYBG4MLBAYG_). Extract all the files in the zip file to "Birdee\dependency\bin"

## Build Birdee with VS!

Add an environment variable "BIRDEE_HOME" and set its value to the absolute path "X:\\XXXX\\Birdee\\BirdeeHome".

Open "Birdee\\Birdee.sln" and build the project "Birdee" and "CoreLibs".

Make a link at "X:\\XXXX\\Birdee\\BirdeeHome\\bin" to the Birdee compiler binary directory "X:\\XXXX\\Birdee\\x64\\DynLLVM\\".
