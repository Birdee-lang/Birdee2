birdeec: Birdee.o CodeGen.o NumberCast.o Preprocessing.o Parser.o MetadataSerializer.o
	$(CXX) -o $(BIN_DIR)/$@ $(CXXFLAGS) -Wl,--start-group $^ -lLLVM-6.0 $(LIBS) -Wl,--end-group
.PHONY:clean
clean:
	# If .o does not exist, don't stop
	rm -f *.o
	rm -f $(BIN_DIR)/birdeec
remake: clean birdeec
