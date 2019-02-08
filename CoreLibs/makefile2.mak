all: birdee list hash_map hash_set vector queue stack tuple fmt unsafe variant
BLIB_DIR=$(BIRDEE_HOME)\blib

$(BLIB_DIR):
	cmd /c "if not exist $@ mkdir $@"
birdee: $(BLIB_DIR) $(BIRDEE_HOME)\src\birdee.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\birdee.txt -o $(BIRDEE_HOME)\blib\birdee.obj --corelib

list: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\list.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\list.txt -o $(BIRDEE_HOME)\blib\list.obj

hash_map: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\hash_map.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\hash_map.txt -o $(BIRDEE_HOME)\blib\hash_map.obj

hash_set: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\hash_set.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\hash_set.txt -o $(BIRDEE_HOME)\blib\hash_set.obj

vector: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\vector.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\vector.txt -o $(BIRDEE_HOME)\blib\vector.obj

queue: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\queue.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\queue.txt -o $(BIRDEE_HOME)\blib\queue.obj

stack: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\stack.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\stack.txt -o $(BIRDEE_HOME)\blib\stack.obj

tuple: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\tuple.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\tuple.txt -o $(BIRDEE_HOME)\blib\tuple.obj

fmt: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\fmt.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\fmt.txt -o $(BIRDEE_HOME)\blib\fmt.obj

unsafe: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\unsafe.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\unsafe.txt -o $(BIRDEE_HOME)\blib\unsafe.obj

variant: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\variant.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\variant.txt -o $(BIRDEE_HOME)\blib\variant.obj

clean:
	del $(BLIB_DIR)\*.obj

remake: clean all