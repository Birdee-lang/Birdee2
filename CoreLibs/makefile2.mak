all: $(BIRDEE_HOME)\blib\birdee.obj libs
BLIB_DIR=$(BIRDEE_HOME)\blib

$(BLIB_DIR):
	cmd /c "if not exist $@ mkdir $@"
$(BIRDEE_HOME)\blib\birdee.obj: $(BLIB_DIR) $(BIRDEE_HOME)\src\birdee.txt
	$(BIRDEE_HOME)\bin\birdeec.exe -i $(BIRDEE_HOME)\src\birdee.txt -o $(BIRDEE_HOME)\blib\birdee.obj --corelib

libs: $(BIRDEE_HOME)\blib\birdee.obj
	python3 $(BIRDEE_HOME)/pylib/bbuild.py -i $(BIRDEE_HOME)/src -o $(BLIB_DIR) \
		variant list hash tuple fmt vector queue stack unsafe concurrent.threading rtti system.io.stdio typedptr reflection concurrent.sync system.io.net \
		functional.lazy extensions.string concurrent.threadpool system.time system.io.stream serialization.json.serializer

clean:
	del $(BLIB_DIR)\*.obj

remake: clean all