all: $(BIN_DIR)/birdeec mklink

mklink: $(LIB_DIR)/libBirdeeBinding.so
	rm -f $(LIB_DIR)/birdeec.so
	ln $(LIB_DIR)/libBirdeeBinding.so $(LIB_DIR)/birdeec.so

$(BIN_DIR)/birdeec: Birdee.o $(LIB_DIR)/libBirdeeCompilerCore.so $(LIB_DIR)/libBirdeeBinding.so
	$(CXX) -o $@ $(CPPFLAGS)  -L$(LIB_DIR) -Wl,--start-group Birdee.o -lBirdeeCompilerCore -ldl -Wl,--end-group -Wl,-rpath='$$ORIGIN/../lib/'
.PHONY:clean mklink remake


$(LIB_DIR)/libBirdeeBinding.so: PythonBinding.o PythonBinding2.o $(LIB_DIR)/libBirdeeCompilerCore.so 
	$(CXX) -shared -o $@ $(CPPFLAGS) -L$(LIB_DIR) -Wl,--start-group PythonBinding.o PythonBinding2.o -ldl $(LIBS) $(PYLIBS) -lBirdeeCompilerCore -Wl,--end-group  -Wl,-rpath='$$ORIGIN'

$(LIB_DIR)/libBirdeeCompilerCore.so: KaleidoscopeJIT.o CodeGen.o NumberCast.o Preprocessing.o Parser.o MetadataSerializer.o MetadataDeserializer.o CopyAST.o BirdeeShared.o Intrinsics.o
	$(CXX) -shared -o $@ $(CPPFLAGS) -Wl,--start-group $^ -lLLVM-6.0 -ldl $(LIBS) -Wl,--end-group -Wl,-rpath='$$ORIGIN'

override CPPFLAGS := -fPIC -fvisibility=hidden -flto=thin $(CPPFLAGS)

clean:
	# If .o does not exist, don't stop
	rm -f *.o
	rm -f $(BIN_DIR)/birdeec
	rm -f $(LIB_DIR)/birdeec.so
	rm -f $(LIB_DIR)/libBirdeeCompilerCore.so
	rm -f $(LIB_DIR)/libBirdeeBinding.so
remake: clean birdeec
