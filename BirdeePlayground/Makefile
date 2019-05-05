all: $(BIN_DIR)/birdeeplay

$(BIN_DIR)/birdeeplay: BirdeePlayground.o
	$(CXX) -o $@ $(CPPFLAGS)  -L$(LIB_DIR) -Wl,--export-dynamic -Wl,--start-group BirdeePlayground.o $(BLIB_DIR)/../blib/birdee.o $(BLIB_DIR)/../blib/hash_map.o $(BLIB_DIR)/../blib/concurrent/threading.o -lBirdeeCompilerCore -lBirdeeRuntime -lLLVM-6.0 -ldl -lpthread -lgc -Wl,--end-group -Wl,-rpath='$$ORIGIN/../lib/'
.PHONY:clean remake


clean:
	# If .o does not exist, don't stop
	rm -f *.o
	rm -f $(BIN_DIR)/birdeeplay
remake: clean $(BIN_DIR)/birdeeplay
