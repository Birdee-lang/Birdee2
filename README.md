# Birdee2
Birdee Language compiler and runtime. Birdee language aims to help construct robust program with minimal efforts and C/C++-level performance. With the help of LLVM, Birdee programs will be compiled into quality native object files (\*.o or \*.obj), which is compatible with native C/C++ linker and compilers (GCC, clang and MSVC). With Birdee, you can have

 * performance ensured by LLVM
 * modern programming language features (classes, lambda expressions, first class functions, ...)
 * easy-to-use meta-programming (templates + compile-time scripts = ?)
 * memory management (GC, ...)

## (Planned) Feature list

### Language features
- [ ] garbage collection
- [x] templates 
- [x] script-based meta-programming
- [x] classes
- [ ] class inheritance, interface 
- [x] array
- [ ] array boundary checking
- [ ] array initializer
- [ ] lambda expressions, functor
- [ ] exceptions
- [ ] threading
- [x] operator overloading
- [x] calling C/C++ code

### Compilation features
- [x] compiling to object files
- [x] generating metadata of object files (auto-generating header files)
- [ ] compiling to portable LLVM IR files
- [ ] JIT execution 
- [ ] Runtime-compilation 


## Documents

Please view our [wiki](https://github.com/Menooker/Birdee2/wiki).

## Building Birdee on Ubuntu

Birdee compiler depends on: LLVM (version 6, newer are not tested), g++ (any version that supports -std=c++14), pybind11 and [nlohmann's JSON library](https://github.com/nlohmann/json). First, you need to update your g++ to make it support C++14 and update git (>=2.0).

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
pip3 install pybind11
sudo ln -s -r /usr/include/llvm-6.0/llvm /usr/include/llvm
sudo ln -s -r /usr/include/llvm-c-6.0/llvm-c/ /usr/include/llvm-c
```

Then fetch Birdee's source code and download other dependencies:
```bash
git clone https://github.com/Menooker/Birdee2
cd Birdee2
mkdir depencency
cd depencency
git init
git remote add -f origin https://github.com/pybind/pybind11
git checkout origin/master include/pybind11
mkdir -p dependency/include/nlohmann
cd dependency/include/nlohmann
wget https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
wget https://raw.githubusercontent.com/nlohmann/fifo_map/master/src/fifo_map.hpp
cd ../../../
```

Finally, make Birdee!

```bash
make
```

One more step, set some required environment variables. You can add something like (assuming that the root directory of Birdee source code is /home/menooker/Birdee2):
```bash
export BIRDEE_HOME=/home/menooker/Birdee2/BirdeeHome/bin/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/menooker/Birdee2/lib
```

to your /etc/profile or ~/.bashrc.

## Building Birdee on with Visual Studio

This repo includes a Visual Studio 2017 solution. Clone this repo and open "Birdee.sln", then you can view the source code of Birdee. However, to compile Birdee, a few more steps needs to be done.

First, you need a copy of LLVM 6.0 (or maybe newer). You can compile LLVM by yourself with CMake, or you can download a pre-compiled LLVM binary built by me.

 * LLVM-Windows-x64-Debug (Debug version with symbols) [BaiduYun](https://pan.baidu.com/s/1Yb4GPKIuYlQcXcKWd7tXRA) [GoogleDrive](https://drive.google.com/open?id=1Jeh8Dm9ca7u119yvsytv_SaHQNqHaq1m)
 * LLVM-Windows-x64-Release (Release version) [BaiduYun](https://pan.baidu.com/s/1JJPzMSNf9XRaSzzHO42DiA) [GoogleDrive](https://drive.google.com/open?id=1TKdx8wxkdvz1Mx2Fzpluc7-XVEa2gaeH)
 * Headers for Windows x64 [BaiduYun](https://pan.baidu.com/s/1kOfgfwvV37VHNa5vwqHciw) [GoogleDrive](https://drive.google.com/open?id=1UONnbLtPzAftrAks9Vhdkb8iDC4rmdqA)


Assume that the root directory of Birdee source code is "Birdee". Then create directory "Birdee\\dependency" and "Birdee\\dependency\\bin".

### Step 1 for VS
Then make sure you have installed an x64 version of Python. Copy/Link "python3.lib" and "python3X.lib" from Python to "Birdee\\dependency\\bin" ("X" in the file name is the exact subversion of your Python). These two files are located in "lib" directory of Python's installed directory.

Run command

```cmd
pip install pybind11
```

Make sure the "pip" program is provided exactly by the same version of Python to be used by Birdee. Finally, link/copy the "include" directory from Python's installed directory to "Birdee\\dependency\\pyinclude"

### Step 2 for VS

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

### Step 3 for VS

Create a directory "Birdee\\dependency\\include" and "Birdee\\dependency\\include\\nlohmann". Download [header](https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp) and [header](https://raw.githubusercontent.com/nlohmann/fifo_map/master/src/fifo_map.hpp) to "Birdee\\dependency\\include\\nlohmann".

### Build Birdee with VS!

Open "Birdee\\Birdee.sln" and build the project "Birdee".