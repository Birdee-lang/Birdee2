all: birdee libs
BLIB_DIR=$(BIRDEE_HOME)\blib

$(BLIB_DIR):
	cmd /c "if not exist $@ mkdir $@"
birdee: $(BLIB_DIR) $(BIRDEE_HOME)\src\birdee.txt
	..\x64\Debug\birdeec.exe -i $(BIRDEE_HOME)\src\birdee.txt -o $(BIRDEE_HOME)\blib\birdee.obj --corelib

libs: birdee
	python3 $(BIRDEE_HOME)/pylib/bbuild.py -i $(BIRDEE_HOME)/src -o $(BLIB_DIR) test variant list hash_map hash_set tuple fmt vector queue stack unsafe concurrent.threading

clean:
	del $(BLIB_DIR)\*.obj

remake: clean all