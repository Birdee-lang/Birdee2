# Birdee2
Birdee Language compiler and runtime. Birdee language aims to help construct robust program with minimal efforts and C/C++-level performance. With the help of LLVM, Birdee programs will be compiled into quality native object files (\*.o or \*.obj), which is compatible with native C/C++ linker and compilers (GCC, clang and MSVC). With Birdee, you can have

 * performance ensured by LLVM
 * modern programming language features (classes, lambda expressions, first class functions, ...)
 * easy-to-use meta-programming (templates + compile-time scripts = ?)
 * memory management (GC, ...)

## (Planned) Feature list

## Language features
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

## Compilation features
- [x] compiling to object files
- [x] generating metadata of object files (auto-generating header files)
- [ ] compiling to portable LLVM IR files
- [ ] JIT execution 
- [ ] Runtime-compilation 
