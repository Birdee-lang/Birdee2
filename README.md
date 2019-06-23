# Birdee2
Birdee Language compiler and runtime. Birdee language aims to help construct robust program with minimal efforts and C/C++-level performance. With the help of LLVM, Birdee programs will be compiled into quality native object files (\*.o or \*.obj), which is compatible with native C/C++ linker and compilers (GCC, clang and MSVC). With Birdee, you can have

 * performance ensured by LLVM
 * modern programming language features (classes, lambda expressions, first class functions, ...)
 * easy-to-use meta-programming (templates + compile-time scripts = ?)
 * memory management (GC, ...)
 * easily try the language using an REPL
 * utilize auto-building tools to compile the code

## Script-based meta-programming

Script-based meta-programming is the most important feature of Birdee. It enables developers to use Python scripts to do the job of the "macro" system and template-based meta-programming of C++, which is powerful but hard to learn and understand. Birdee allows Python scripts to be embedded in Birdee code and the script will be run at compile time. The embeded script is surrounded by a pair of {@ ... @} .The script can:
 
 * Generate Birdee code - Like what "macro" did in C++:
```vb
{@
py_var = 0
def counter():
  global py_var
  py_var += 1
  return str(py_var)
@}
dim variable1 as int = {@ set_ast(expr(counter())) @}
dim variable2 as int = {@ set_ast(expr(counter())) @}
```
The above Birdee code defines a compile-time Python function which runs as a compile-time counter. The script "set_ast(expr(counter()))" will call the python function to get strings "1", "2", "3", ..., and it then generate a Birdee expression with the string. 

 * Compile-time meta-programming - Get types/members/names/... of variables/classes/... to automatically generate different code
```vb
function hash[T](v as T) as uint
	return {@
ty=resolve_type("T")
if ty.is_integer():
	set_ast(expr("int_hash(v)"))
elif ty.index_level==0 and ty.base==BasicType.CLASS:
	set_ast(expr("v.__hash__()"))
else:
	raise Exception(f"Cannot hash the type: {ty}")
@}
end
```
The above "hash" template function generates hash functions for different types.

 * Annotate a part of code - Just like Java's annotation, but only better. Birdee's annotation scripts can change the existing code
```vb
{@
def change_str(stmt):
    if isinstance(stmt, StringLiteralAST):
      stmt.value = stmt.value + "_test"
    else
      stmt.run(change_str)
@}

@change_str
function print_hello()
  println("hello")
end
```
The annotation "change\_str" will change the string "hello" to "hello_test".

Another interesting feature of Birdee is that the compiler itself can be invoked as a library of Python. You can run Python code to generate Birdee code:

```python
from birdeec import *
top_level('''
dim a =true
dim b =1
while a
	a= b%2==0
end while
	''')
process_top_level()
clear_compile_unit()
```

## (Planned) Feature list

### Language features
- [x] garbage collection
- [x] templates 
- [x] script-based meta-programming
- [x] classes
- [ ] class inheritance, interface 
- [x] array
- [ ] array boundary checking
- [x] array initializer
- [x] lambda expressions
- [x] exceptions
- [x] threading
- [x] operator overloading
- [x] calling C/C++ code

### Compilation features
- [x] compiling to object files
- [x] generating metadata of object files (auto-generating header files)
- [ ] compiling to portable LLVM IR files
- [ ] JIT execution 
- [ ] Runtime-compilation 


## Documents

Please view our [documents](https://birdee-lang.github.io/Doc/).

## Installing pre-built binaries of Birdee compiler and libraries

Please follow the [link](https://github.com/Birdee-lang/Birdee2/releases) to download the pre-built Birdee.

For Windows version, just unzip and add the unzipped directory to the environment variable as `BIRDEE_HOME`. In the directory pointed by `BIRDEE_HOME`, there should be directories named `bin`, `pylib`, `blib` and `src`.  Also, Birdee compiler depends on a specific version of Python (e.g. 3.7).

For Linux Debian systems, use `sudo dpkg -i birdee_xxxx.deb` to install and `sudo dpkg -r birdee` to remove. This package requires libpython3.7-dev, libgc-dev and LLVM-6.0 to be installed. Remember to add `/usr/bin/birdee0.1` to the environment variable as `BIRDEE_HOME`.

## Compiling Birdee from source code

Please follow the instructions in the [link](https://github.com/Birdee-lang/Birdee2/blob/master/BUILDME.md).

## What's in the Birdee toolkit?

Birdee provides several useful tools to help develop Birdee programs.

### Birdee compiler

The compiler compiles a single Birdee module source file (\*.bdm) into an native object file (\*.o or \*.obj) or LLVM IR file. The outcome of the compiler can be further linked into an executable file by external linkers (e.g. ld on Linux, link.exe of MSVC). Please note that Birdee compiler alone will not help you link the Birdee modules. You should link the modules by yourself or by the auto-building tool "bbuild". For more information on the compiler command line arguments, please refer to [here](https://birdee-lang.github.io/Doc/Tools/Compiler-command-line-mannual/).

Besides the object files, the Birdee compiler will generate an meta-data file (\*.bmm), for the compiled module, which is in JSON format. It contains information on the exported variables, classes and the function definitions of the module. This overcomes the issue that native object files will lose the important meta-data of the source code.

### Birdee auto-building tool - bbuild

bbuild is a Python-based auto-building tool provided by Birdee, to compile multiple Birdee modules and optionally link them into an executable/shared object file. It can

 * Parse the `import` dependency of the Birdee modules, and automatically find the source code (or compiled object files) of dependent modules.
 * Automatically compile the modules from the source files.
 * Optionally link the object files into an executable/shared object file

For more information on bbuild, please follow the [link](https://birdee-lang.github.io/Doc/Tools/bbuild/)

### Birdee playground (REPL)

To help to learn Birdee language and try Birdee programs, we provide a playground program, where you can simply type Birdee code and instantly see the results without compiling the whole program using the compiler. For each line of code you input, the playground will compile and execute it and prints the execution results on the console. Please follow the [link](https://birdee-lang.github.io/Doc/Tools/Birdee-playground-(REPL)/) to try it.

## Tips on running compiled Birdee programs

On Linux, a compiled Birdee application has an additional dependency on libgc. Please make sure it is installed on the system where the application runs.

### Run Birdee programs on Windows!

To run Birdee programs that are compiled with Birdee compiler on Windows, make sure "gc_64.dll" is in DLL search path (e.g. where the ".exe" file is). If you use exceptions in your Birdee program, make sure "libgcc_s_seh-1.dll" and "libwinpthread-1.dll" are also in DLL search path.

If you use the pre-compiled binaries of Birdee, these DLLs can be found in the "bin" directory. These DLL files can also be downloaded via the following links. [BaiduYun](https://pan.baidu.com/s/1FWnHpQkxj5PC4DP1PEMRlg) or [GoogleDrive](https://drive.google.com/open?id=1GIH-YDe2IFMnaYE91uOXAJlnXJjX3PvT). Note that "libgcc_s_seh-1.dll" and "libwinpthread-1.dll" are extracted from mingw64.

