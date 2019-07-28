all: $(BIN_DIR)/birdeeplay

$(BIN_DIR)/birdeeplay: BirdeePlayground.o
	$(CXX) -o $@ $(CPPFLAGS)  -L$(LIB_DIR) -Wl,--export-dynamic -Wl,--start-group BirdeePlayground.o $(BLIB_DIR)/../blib/*.o $(BLIB_DIR)/../blib/concurrent/threading.o  $(BLIB_DIR)/../blib/system/io/file.o \
$(BLIB_DIR)/../blib/system/io/filedef.o $(BLIB_DIR)/../blib/system/io/stdio.o $(BLIB_DIR)/../blib/system/specific/unistd/file.o -lBirdeeCompilerCore -lBirdeeRuntime -lLLVM-6.0 -ldl -lpthread -lgc -Wl,--end-group -Wl,-rpath='$$ORIGIN/../lib/'
.PHONY:clean remake


clean:
	# If .o does not exist, don't stop
	rm -f *.o
	rm -f $(BIN_DIR)/birdeeplay
remake: clean $(BIN_DIR)/birdeeplay
