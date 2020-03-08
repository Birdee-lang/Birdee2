all: $(LIB_DIR)/libBirdeeRuntime.a $(LIB_DIR)/dllmain.cpp
$(LIB_DIR)/libBirdeeRuntime.a: BirdeeRuntime.o BirdeeException.o
	ar -crv $@ $^
$(LIB_DIR)/dllmain.cpp: DLLMain.cpp
	cp DLLMain.cpp $(LIB_DIR)/dllmain.cpp
.PHONY:clean
override CPPFLAGS := -fPIC $(CPPFLAGS)
clean:
	# If .o does not exist, don't stop
	rm -f *.o
	rm -f $(LIB_DIR)/libBirdeeRuntime.a
	rm -f $(LIB_DIR)/dllmain.cpp
remake: clean $(LIB_DIR)/libBirdeeRuntime.a $(LIB_DIR)/dllmain.cpp
