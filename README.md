# Birdee2
Birdee Language compiler and runtime. Birdee language aims to help construct robust program with minimal efforts and C/C++-level performance. With the help of LLVM, Birdee programs will be compiled into quality native object files (\*.o or \*.obj), which is compatible with native C/C++ linker and compilers (GCC, clang and MSVC). With Birdee, you can have

 * performance ensured by LLVM
 * modern programming language features (classes, lambda expressions, first class functions, ...)
 * easy-to-use meta-programming (templates + compile-time scripts = ?)
 * memory management (GC, ...)

## (Planned) Feature list

### Language features
- [ ] garbage collection
- [ ] templates 
- [ ] script-based meta-programming
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

