extern "C"{
void AddFunctions(llvm::orc::KaleidoscopeJIT* jit){
extern void hash_0int__hash();
jit->addNative("hash_0int__hash",hash_0int__hash);

extern void hash_0_1main();
jit->addNative("hash_0_1main",hash_0_1main);

extern void concurrent_0threading_0sleep();
jit->addNative("concurrent_0threading_0sleep",concurrent_0threading_0sleep);

extern void concurrent_0threading_0do__exit__thread();
jit->addNative("concurrent_0threading_0do__exit__thread",concurrent_0threading_0do__exit__thread);

extern void concurrent_0threading_0do__create__thread();
jit->addNative("concurrent_0threading_0do__create__thread",concurrent_0threading_0do__create__thread);

extern void concurrent_0threading_0_1main();
jit->addNative("concurrent_0threading_0_1main",concurrent_0threading_0_1main);

extern void string__buffer_0string__buffer_0to__str();
jit->addNative("string__buffer_0string__buffer_0to__str",string__buffer_0string__buffer_0to__str);

extern void string__buffer_0string__buffer_0____init____();
jit->addNative("string__buffer_0string__buffer_0____init____",string__buffer_0string__buffer_0____init____);

extern void string__buffer_0string__buffer_0append();
jit->addNative("string__buffer_0string__buffer_0append",string__buffer_0string__buffer_0append);

extern void string__buffer_0_1main();
jit->addNative("string__buffer_0_1main",string__buffer_0_1main);

extern void system_0io_0file_0__file_0close();
jit->addNative("system_0io_0file_0__file_0close",system_0io_0file_0__file_0close);

extern void system_0io_0file_0__file_0get__native();
jit->addNative("system_0io_0file_0__file_0get__native",system_0io_0file_0__file_0get__native);

extern void system_0io_0file_0__file_0set__native();
jit->addNative("system_0io_0file_0__file_0set__native",system_0io_0file_0__file_0set__native);

extern void system_0io_0file_0__file_0____del____();
jit->addNative("system_0io_0file_0__file_0____del____",system_0io_0file_0__file_0____del____);

extern void system_0io_0file_0__file_0read();
jit->addNative("system_0io_0file_0__file_0read",system_0io_0file_0__file_0read);

extern void system_0io_0file_0__file_0write();
jit->addNative("system_0io_0file_0__file_0write",system_0io_0file_0__file_0write);

extern void system_0io_0file_0__file_0writestr();
jit->addNative("system_0io_0file_0__file_0writestr",system_0io_0file_0__file_0writestr);

extern void system_0io_0file_0__file_0seek();
jit->addNative("system_0io_0file_0__file_0seek",system_0io_0file_0__file_0seek);

extern void system_0io_0file_0__file_0____init____();
jit->addNative("system_0io_0file_0__file_0____init____",system_0io_0file_0__file_0____init____);

extern void system_0io_0file_0input__file_0____del____();
jit->addNative("system_0io_0file_0input__file_0____del____",system_0io_0file_0input__file_0____del____);

extern void system_0io_0file_0input__file_0write();
jit->addNative("system_0io_0file_0input__file_0write",system_0io_0file_0input__file_0write);

extern void system_0io_0file_0input__file_0writestr();
jit->addNative("system_0io_0file_0input__file_0writestr",system_0io_0file_0input__file_0writestr);

extern void system_0io_0file_0input__file_0____init____();
jit->addNative("system_0io_0file_0input__file_0____init____",system_0io_0file_0input__file_0____init____);

extern void system_0io_0file_0output__file_0____del____();
jit->addNative("system_0io_0file_0output__file_0____del____",system_0io_0file_0output__file_0____del____);

extern void system_0io_0file_0output__file_0read();
jit->addNative("system_0io_0file_0output__file_0read",system_0io_0file_0output__file_0read);

extern void system_0io_0file_0output__file_0____init____();
jit->addNative("system_0io_0file_0output__file_0____init____",system_0io_0file_0output__file_0____init____);

extern void system_0io_0file_0file_0____del____();
jit->addNative("system_0io_0file_0file_0____del____",system_0io_0file_0file_0____del____);

extern void system_0io_0file_0file_0____init____();
jit->addNative("system_0io_0file_0file_0____init____",system_0io_0file_0file_0____init____);

extern void system_0io_0file_0string__scanner_0____init____();
jit->addNative("system_0io_0file_0string__scanner_0____init____",system_0io_0file_0string__scanner_0____init____);

extern void system_0io_0file_0string__scanner_0read();
jit->addNative("system_0io_0file_0string__scanner_0read",system_0io_0file_0string__scanner_0read);

extern void system_0io_0file_0string__scanner_0get__char();
jit->addNative("system_0io_0file_0string__scanner_0get__char",system_0io_0file_0string__scanner_0get__char);

extern void system_0io_0file_0string__scanner_0get__until();
jit->addNative("system_0io_0file_0string__scanner_0get__until",system_0io_0file_0string__scanner_0get__until);

extern void system_0io_0file_0string__scanner_0get__line();
jit->addNative("system_0io_0file_0string__scanner_0get__line",system_0io_0file_0string__scanner_0get__line);

extern void system_0io_0file_0_1main();
jit->addNative("system_0io_0file_0_1main",system_0io_0file_0_1main);

extern void system_0io_0filedef_0file__open__exception_0____init____();
jit->addNative("system_0io_0filedef_0file__open__exception_0____init____",system_0io_0filedef_0file__open__exception_0____init____);

extern void system_0io_0filedef_0file__open__exception_0get__message();
jit->addNative("system_0io_0filedef_0file__open__exception_0get__message",system_0io_0filedef_0file__open__exception_0get__message);

extern void system_0io_0filedef_0file__exception_0____init____();
jit->addNative("system_0io_0filedef_0file__exception_0____init____",system_0io_0filedef_0file__exception_0____init____);

extern void system_0io_0filedef_0file__exception_0get__message();
jit->addNative("system_0io_0filedef_0file__exception_0get__message",system_0io_0filedef_0file__exception_0get__message);

extern void system_0io_0filedef_0_1main();
jit->addNative("system_0io_0filedef_0_1main",system_0io_0filedef_0_1main);

extern void system_0io_0stdio_0_1main();
jit->addNative("system_0io_0stdio_0_1main",system_0io_0stdio_0_1main);

extern void system_0specific_0win32_0file_0__fwrite();
jit->addNative("system_0specific_0win32_0file_0__fwrite",system_0specific_0win32_0file_0__fwrite);

extern void system_0specific_0win32_0file_0__is__good__handle();
jit->addNative("system_0specific_0win32_0file_0__is__good__handle",system_0specific_0win32_0file_0__is__good__handle);

extern void system_0specific_0win32_0file_0__fopen();
jit->addNative("system_0specific_0win32_0file_0__fopen",system_0specific_0win32_0file_0__fopen);

extern void system_0specific_0win32_0file_0__fclose();
jit->addNative("system_0specific_0win32_0file_0__fclose",system_0specific_0win32_0file_0__fclose);

extern void system_0specific_0win32_0file_0__fread();
jit->addNative("system_0specific_0win32_0file_0__fread",system_0specific_0win32_0file_0__fread);

extern void system_0specific_0win32_0file_0__fseek();
jit->addNative("system_0specific_0win32_0file_0__fseek",system_0specific_0win32_0file_0__fseek);

extern void system_0specific_0win32_0file_0__getstd();
jit->addNative("system_0specific_0win32_0file_0__getstd",system_0specific_0win32_0file_0__getstd);

extern void system_0specific_0win32_0file_0_1main();
jit->addNative("system_0specific_0win32_0file_0_1main",system_0specific_0win32_0file_0_1main);

extern void birdee_0print();
jit->addNative("birdee_0print",birdee_0print);

extern void birdee_0int2str();
jit->addNative("birdee_0int2str",birdee_0int2str);

extern void birdee_0bool2str();
jit->addNative("birdee_0bool2str",birdee_0bool2str);

extern void birdee_0println();
jit->addNative("birdee_0println",birdee_0println);

extern void birdee_0____create__basic__exception__no__call();
jit->addNative("birdee_0____create__basic__exception__no__call",birdee_0____create__basic__exception__no__call);

extern void birdee_0genericarray_0length();
jit->addNative("birdee_0genericarray_0length",birdee_0genericarray_0length);

extern void birdee_0genericarray_0get__raw();
jit->addNative("birdee_0genericarray_0get__raw",birdee_0genericarray_0get__raw);

extern void birdee_0string_0length();
jit->addNative("birdee_0string_0length",birdee_0string_0length);

extern void birdee_0string_0____add____();
jit->addNative("birdee_0string_0____add____",birdee_0string_0____add____);

extern void birdee_0string_0____init____();
jit->addNative("birdee_0string_0____init____",birdee_0string_0____init____);

extern void birdee_0string_0____hash____();
jit->addNative("birdee_0string_0____hash____",birdee_0string_0____hash____);

extern void birdee_0string_0____eq____();
jit->addNative("birdee_0string_0____eq____",birdee_0string_0____eq____);

extern void birdee_0string_0____ne____();
jit->addNative("birdee_0string_0____ne____",birdee_0string_0____ne____);

extern void birdee_0string_0copy__bytes();
jit->addNative("birdee_0string_0copy__bytes",birdee_0string_0copy__bytes);

extern void birdee_0string_0get__bytes();
jit->addNative("birdee_0string_0get__bytes",birdee_0string_0get__bytes);

extern void birdee_0string_0get__raw();
jit->addNative("birdee_0string_0get__raw",birdee_0string_0get__raw);

extern void birdee_0string_0____getitem____();
jit->addNative("birdee_0string_0____getitem____",birdee_0string_0____getitem____);

extern void birdee_0type__info_0get__name();
jit->addNative("birdee_0type__info_0get__name",birdee_0type__info_0get__name);

extern void birdee_0type__info_0get__parent();
jit->addNative("birdee_0type__info_0get__parent",birdee_0type__info_0get__parent);

extern void birdee_0type__info_0is__parent__of();
jit->addNative("birdee_0type__info_0is__parent__of",birdee_0type__info_0is__parent__of);

extern void birdee_0runtime__exception_0____init____();
jit->addNative("birdee_0runtime__exception_0____init____",birdee_0runtime__exception_0____init____);

extern void birdee_0runtime__exception_0get__message();
jit->addNative("birdee_0runtime__exception_0get__message",birdee_0runtime__exception_0get__message);

extern void birdee_0_1main();
jit->addNative("birdee_0_1main",birdee_0_1main);

}}
