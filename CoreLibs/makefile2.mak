all: birdee list hash_map vector tuple fmt
BLIB_DIR=$(BIRDEE_HOME)\blib: 

$(BLIB_DIR):
	mkdir $(BIRDEE_HOME)\blib
birdee: $(BLIB_DIR) $(BIRDEE_HOME)\src\birdee.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\birdee.txt -o $(BIRDEE_HOME)\blib\birdee.obj --corelib

list: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\list.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\list.txt -o $(BIRDEE_HOME)\blib\list.obj

hash_map: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\hash_map.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\hash_map.txt -o $(BIRDEE_HOME)\blib\hash_map.obj

vector: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\vector.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\vector.txt -o $(BIRDEE_HOME)\blib\vector.obj

tuple: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\tuple.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\tuple.txt -o $(BIRDEE_HOME)\blib\tuple.obj

fmt: $(BLIB_DIR) birdee $(BIRDEE_HOME)\src\fmt.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\fmt.txt -o $(BIRDEE_HOME)\blib\fmt.obj

clean:
	del $(BLIB_DIR)\*.obj

remake: clean all