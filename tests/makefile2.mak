
all: $(OUT_DIR) class_inherit_import.test container_test.test exception_test.test threading_test.test
SRC_DIR=.
OUT_DIR=bin


$(OUT_DIR):
	cmd /c "if not exist $@ mkdir $@"

.SUFFIXES : .test .txt .bdm

.txt.test: 
	python3 $(BIRDEE_HOME)\pylib\bbuild.py -i $(SRC_DIR) -o $(OUT_DIR) -le $(OUT_DIR)\$*.exe $*
	$(OUT_DIR)\$*.exe

.bdm.test: 
	python3 $(BIRDEE_HOME)\pylib\bbuild.py -i $(SRC_DIR) -o $(OUT_DIR) -le $(OUT_DIR)\$*.exe $*
	$(OUT_DIR)\$*.exe

clean:
	del $(OUT_DIR)\*.bmm
	del $(OUT_DIR)\*.obj
	del $(OUT_DIR)\*.exe

remake: clean all