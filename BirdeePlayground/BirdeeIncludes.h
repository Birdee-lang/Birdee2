extern "C"{
void AddFunctions(llvm::orc::KaleidoscopeJIT* jit){
extern void extensions_0string_0ptr__add();
jit->addNative("extensions_0string_0ptr__add",extensions_0string_0ptr__add);

extern void extensions_0string_0ptr__diff();
jit->addNative("extensions_0string_0ptr__diff",extensions_0string_0ptr__diff);

extern void extensions_0string_0string______mul____();
jit->addNative("extensions_0string_0string______mul____",extensions_0string_0string______mul____);

extern void extensions_0string_0check__index();
jit->addNative("extensions_0string_0check__index",extensions_0string_0check__index);

extern void extensions_0string_0string1__view();
jit->addNative("extensions_0string_0string1__view",extensions_0string_0string1__view);

extern void extensions_0string_0string__viewall();
jit->addNative("extensions_0string_0string__viewall",extensions_0string_0string__viewall);

extern void extensions_0string_0string__findview();
jit->addNative("extensions_0string_0string__findview",extensions_0string_0string__findview);

extern void extensions_0string_0string__find();
jit->addNative("extensions_0string_0string__find",extensions_0string_0string__find);

extern void extensions_0string_0string__splitview();
jit->addNative("extensions_0string_0string__splitview",extensions_0string_0string__splitview);

extern void extensions_0string_0string__split();
jit->addNative("extensions_0string_0string__split",extensions_0string_0string__split);

extern void extensions_0string_0string__to__int();
jit->addNative("extensions_0string_0string__to__int",extensions_0string_0string__to__int);

extern void extensions_0string_0string__to__double();
jit->addNative("extensions_0string_0string__to__double",extensions_0string_0string__to__double);

extern void extensions_0string_0string__to__long();
jit->addNative("extensions_0string_0string__to__long",extensions_0string_0string__to__long);

extern void extensions_0string_0string__view_0____init____();
jit->addNative("extensions_0string_0string__view_0____init____",extensions_0string_0string__view_0____init____);

extern void extensions_0string_0string__view_0____deref____();
jit->addNative("extensions_0string_0string__view_0____deref____",extensions_0string_0string__view_0____deref____);

extern void extensions_0string_0string__view_0____getitem____();
jit->addNative("extensions_0string_0string__view_0____getitem____",extensions_0string_0string__view_0____getitem____);

extern void extensions_0string_0string__view_0length();
jit->addNative("extensions_0string_0string__view_0length",extensions_0string_0string__view_0length);

extern void extensions_0string_0string__view_0get__raw();
jit->addNative("extensions_0string_0string__view_0get__raw",extensions_0string_0string__view_0get__raw);

extern void extensions_0string_0string__view_0view();
jit->addNative("extensions_0string_0string__view_0view",extensions_0string_0string__view_0view);

extern void extensions_0string_0string__view_0find();
jit->addNative("extensions_0string_0string__view_0find",extensions_0string_0string__view_0find);

extern void extensions_0string_0string__view_0split();
jit->addNative("extensions_0string_0string__view_0split",extensions_0string_0string__view_0split);

extern void extensions_0string_0string__view_0____eq____();
jit->addNative("extensions_0string_0string__view_0____eq____",extensions_0string_0string__view_0____eq____);

extern void extensions_0string_0string__view_0____ne____();
jit->addNative("extensions_0string_0string__view_0____ne____",extensions_0string_0string__view_0____ne____);

extern void extensions_0string_0string__view_0to__int();
jit->addNative("extensions_0string_0string__view_0to__int",extensions_0string_0string__view_0to__int);

extern void extensions_0string_0string__view_0to__long();
jit->addNative("extensions_0string_0string__view_0to__long",extensions_0string_0string__view_0to__long);

extern void extensions_0string_0string__view_0to__double();
jit->addNative("extensions_0string_0string__view_0to__double",extensions_0string_0string__view_0to__double);

extern void extensions_0string_0_1main();
jit->addNative("extensions_0string_0_1main",extensions_0string_0_1main);

extern void hash_0int__hash();
jit->addNative("hash_0int__hash",hash_0int__hash);

extern void hash_0_1main();
jit->addNative("hash_0_1main",hash_0_1main);

extern void concurrent_0threading_0sleep();
jit->addNative("concurrent_0threading_0sleep",concurrent_0threading_0sleep);

extern void concurrent_0threading_0thread_0join();
jit->addNative("concurrent_0threading_0thread_0join",concurrent_0threading_0thread_0join);

extern void concurrent_0threading_0thread_0____del____();
jit->addNative("concurrent_0threading_0thread_0____del____",concurrent_0threading_0thread_0____del____);

extern void concurrent_0threading_0_1main();
jit->addNative("concurrent_0threading_0_1main",concurrent_0threading_0_1main);

extern void concurrent_0sync_0create__poller();
jit->addNative("concurrent_0sync_0create__poller",concurrent_0sync_0create__poller);

extern void concurrent_0sync_0mutex_0____init____();
jit->addNative("concurrent_0sync_0mutex_0____init____",concurrent_0sync_0mutex_0____init____);

extern void concurrent_0sync_0mutex_0acquire();
jit->addNative("concurrent_0sync_0mutex_0acquire",concurrent_0sync_0mutex_0acquire);

extern void concurrent_0sync_0mutex_0release();
jit->addNative("concurrent_0sync_0mutex_0release",concurrent_0sync_0mutex_0release);

extern void concurrent_0sync_0mutex_0____del____();
jit->addNative("concurrent_0sync_0mutex_0____del____",concurrent_0sync_0mutex_0____del____);

extern void concurrent_0sync_0_1main();
jit->addNative("concurrent_0sync_0_1main",concurrent_0sync_0_1main);

extern void concurrent_0syncdef_0awaitable_0do__then();
jit->addNative("concurrent_0syncdef_0awaitable_0do__then",concurrent_0syncdef_0awaitable_0do__then);

extern void concurrent_0syncdef_0awaitable_0do__await();
jit->addNative("concurrent_0syncdef_0awaitable_0do__await",concurrent_0syncdef_0awaitable_0do__await);

extern void concurrent_0syncdef_0awaitable_0get__native__handle();
jit->addNative("concurrent_0syncdef_0awaitable_0get__native__handle",concurrent_0syncdef_0awaitable_0get__native__handle);

extern void concurrent_0syncdef_0awaitable_0close();
jit->addNative("concurrent_0syncdef_0awaitable_0close",concurrent_0syncdef_0awaitable_0close);

extern void concurrent_0syncdef_0wait__exception_0____init____();
jit->addNative("concurrent_0syncdef_0wait__exception_0____init____",concurrent_0syncdef_0wait__exception_0____init____);

extern void concurrent_0syncdef_0wait__exception_0get__message();
jit->addNative("concurrent_0syncdef_0wait__exception_0get__message",concurrent_0syncdef_0wait__exception_0get__message);

extern void concurrent_0syncdef_0awaitable__wrapper_0____init____();
jit->addNative("concurrent_0syncdef_0awaitable__wrapper_0____init____",concurrent_0syncdef_0awaitable__wrapper_0____init____);

extern void concurrent_0syncdef_0awaitable__wrapper_0____hash____();
jit->addNative("concurrent_0syncdef_0awaitable__wrapper_0____hash____",concurrent_0syncdef_0awaitable__wrapper_0____hash____);

extern void concurrent_0syncdef_0awaitable__wrapper_0____eq____();
jit->addNative("concurrent_0syncdef_0awaitable__wrapper_0____eq____",concurrent_0syncdef_0awaitable__wrapper_0____eq____);

extern void concurrent_0syncdef_0_1main();
jit->addNative("concurrent_0syncdef_0_1main",concurrent_0syncdef_0_1main);

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

extern void system_0io_0stdio_0stderr();
jit->addNative("system_0io_0stdio_0stderr",system_0io_0stdio_0stderr);

extern void system_0io_0stdio_0stdin();
jit->addNative("system_0io_0stdio_0stdin",system_0io_0stdio_0stdin);

extern void system_0io_0stdio_0stdout();
jit->addNative("system_0io_0stdio_0stdout",system_0io_0stdio_0stdout);

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

extern void system_0specific_0win32_0concurrent_0event__cache__size();
jit->addNative("system_0specific_0win32_0concurrent_0event__cache__size",system_0specific_0win32_0concurrent_0event__cache__size);

extern void system_0specific_0win32_0concurrent_0event__cache__lock();
jit->addNative("system_0specific_0win32_0concurrent_0event__cache__lock",system_0specific_0win32_0concurrent_0event__cache__lock);

extern void system_0specific_0win32_0concurrent_0event__cache();
jit->addNative("system_0specific_0win32_0concurrent_0event__cache",system_0specific_0win32_0concurrent_0event__cache);

extern void system_0specific_0win32_0concurrent_0__sleep();
jit->addNative("system_0specific_0win32_0concurrent_0__sleep",system_0specific_0win32_0concurrent_0__sleep);

extern void system_0specific_0win32_0concurrent_0recycle__event();
jit->addNative("system_0specific_0win32_0concurrent_0recycle__event",system_0specific_0win32_0concurrent_0recycle__event);

extern void system_0specific_0win32_0concurrent_0get__event();
jit->addNative("system_0specific_0win32_0concurrent_0get__event",system_0specific_0win32_0concurrent_0get__event);

extern void system_0specific_0win32_0concurrent_0do__wait();
jit->addNative("system_0specific_0win32_0concurrent_0do__wait",system_0specific_0win32_0concurrent_0do__wait);

extern void system_0specific_0win32_0concurrent_0do__create__thread();
jit->addNative("system_0specific_0win32_0concurrent_0do__create__thread",system_0specific_0win32_0concurrent_0do__create__thread);

extern void system_0specific_0win32_0concurrent_0do__exit__thread();
jit->addNative("system_0specific_0win32_0concurrent_0do__exit__thread",system_0specific_0win32_0concurrent_0do__exit__thread);

extern void system_0specific_0win32_0concurrent_0__close__thread();
jit->addNative("system_0specific_0win32_0concurrent_0__close__thread",system_0specific_0win32_0concurrent_0__close__thread);

extern void system_0specific_0win32_0concurrent_0__join__thread();
jit->addNative("system_0specific_0win32_0concurrent_0__join__thread",system_0specific_0win32_0concurrent_0__join__thread);

extern void system_0specific_0win32_0concurrent_0__await__multiple__awaitables__with__buffer();
jit->addNative("system_0specific_0win32_0concurrent_0__await__multiple__awaitables__with__buffer",system_0specific_0win32_0concurrent_0__await__multiple__awaitables__with__buffer);

extern void system_0specific_0win32_0concurrent_0__await__multiple__awaitables();
jit->addNative("system_0specific_0win32_0concurrent_0__await__multiple__awaitables",system_0specific_0win32_0concurrent_0__await__multiple__awaitables);

extern void system_0specific_0win32_0concurrent_0mutex__t__kernel_0init();
jit->addNative("system_0specific_0win32_0concurrent_0mutex__t__kernel_0init",system_0specific_0win32_0concurrent_0mutex__t__kernel_0init);

extern void system_0specific_0win32_0concurrent_0mutex__t__kernel_0enter();
jit->addNative("system_0specific_0win32_0concurrent_0mutex__t__kernel_0enter",system_0specific_0win32_0concurrent_0mutex__t__kernel_0enter);

extern void system_0specific_0win32_0concurrent_0mutex__t__kernel_0leave();
jit->addNative("system_0specific_0win32_0concurrent_0mutex__t__kernel_0leave",system_0specific_0win32_0concurrent_0mutex__t__kernel_0leave);

extern void system_0specific_0win32_0concurrent_0mutex__t__kernel_0del();
jit->addNative("system_0specific_0win32_0concurrent_0mutex__t__kernel_0del",system_0specific_0win32_0concurrent_0mutex__t__kernel_0del);

extern void system_0specific_0win32_0concurrent_0mutex__t_0init();
jit->addNative("system_0specific_0win32_0concurrent_0mutex__t_0init",system_0specific_0win32_0concurrent_0mutex__t_0init);

extern void system_0specific_0win32_0concurrent_0mutex__t_0enter();
jit->addNative("system_0specific_0win32_0concurrent_0mutex__t_0enter",system_0specific_0win32_0concurrent_0mutex__t_0enter);

extern void system_0specific_0win32_0concurrent_0mutex__t_0leave();
jit->addNative("system_0specific_0win32_0concurrent_0mutex__t_0leave",system_0specific_0win32_0concurrent_0mutex__t_0leave);

extern void system_0specific_0win32_0concurrent_0mutex__t_0del();
jit->addNative("system_0specific_0win32_0concurrent_0mutex__t_0del",system_0specific_0win32_0concurrent_0mutex__t_0del);

extern void system_0specific_0win32_0concurrent_0__poller_0____init____();
jit->addNative("system_0specific_0win32_0concurrent_0__poller_0____init____",system_0specific_0win32_0concurrent_0__poller_0____init____);

extern void system_0specific_0win32_0concurrent_0__poller_0perpare__handle__buffer();
jit->addNative("system_0specific_0win32_0concurrent_0__poller_0perpare__handle__buffer",system_0specific_0win32_0concurrent_0__poller_0perpare__handle__buffer);

extern void system_0specific_0win32_0concurrent_0__poller_0add();
jit->addNative("system_0specific_0win32_0concurrent_0__poller_0add",system_0specific_0win32_0concurrent_0__poller_0add);

extern void system_0specific_0win32_0concurrent_0__poller_0replace();
jit->addNative("system_0specific_0win32_0concurrent_0__poller_0replace",system_0specific_0win32_0concurrent_0__poller_0replace);

extern void system_0specific_0win32_0concurrent_0__poller_0remove();
jit->addNative("system_0specific_0win32_0concurrent_0__poller_0remove",system_0specific_0win32_0concurrent_0__poller_0remove);

extern void system_0specific_0win32_0concurrent_0__poller_0await();
jit->addNative("system_0specific_0win32_0concurrent_0__poller_0await",system_0specific_0win32_0concurrent_0__poller_0await);

extern void system_0specific_0win32_0concurrent_0_1main();
jit->addNative("system_0specific_0win32_0concurrent_0_1main",system_0specific_0win32_0concurrent_0_1main);

extern void birdee_0bool2str();
jit->addNative("birdee_0bool2str",birdee_0bool2str);

extern void birdee_0print();
jit->addNative("birdee_0print",birdee_0print);

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
