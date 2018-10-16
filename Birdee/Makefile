all: birdeec mklink

mklink: $(LIB_DIR)/libBirdeeBinding.so
	rm -f $(LIB_DIR)/birdeec.so
	ln $(LIB_DIR)/libBirdeeBinding.so $(LIB_DIR)/birdeec.so

birdeec: Birdee.o $(LIB_DIR)/libBirdeeCompilerCore.so $(LIB_DIR)/libBirdeeBinding.so
	$(CXX) -o $(BIN_DIR)/$@ $(CPPFLAGS)  -L$(LIB_DIR) -Wl,--start-group Birdee.o -lBirdeeCompilerCore -ldl -Wl,--end-group
.PHONY:clean


$(LIB_DIR)/libBirdeeBinding.so: PythonBinding.o PythonBinding2.o $(LIB_DIR)/libBirdeeCompilerCore.so 
	$(CXX) -shared -o $@ $(CPPFLAGS) -L$(LIB_DIR) -Wl,--start-group PythonBinding.o PythonBinding2.o $(LIBS) $(PYLIBS) -lBirdeeCompilerCore -Wl,--end-group 

$(LIB_DIR)/libBirdeeCompilerCore.so: CodeGen.o NumberCast.o Preprocessing.o Parser.o MetadataSerializer.o MetadataDeserializer.o CopyAST.o BirdeeShared.o
	$(CXX) -shared -o $@ $(CPPFLAGS) -Wl,--start-group $^ -lLLVM-6.0 $(LIBS) -Wl,--end-group

override CPPFLAGS := -fPIC $(CPPFLAGS)

clean:
	# If .o does not exist, don't stop
	rm -f *.o
	rm -f $(BIN_DIR)/birdeec
	rm -f $(LIB_DIR)/birdeec.so
	rm -f $(LIB_DIR)/libBirdeeCompilerCore.so
	rm -f $(LIB_DIR)/libBirdeeBinding.so
remake: clean birdeec
