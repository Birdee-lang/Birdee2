$(LIB_DIR)/libBirdeeRuntime.a: BirdeeRuntime.o
	ar -crv $@ $^
.PHONY:clean
clean:
	# If .o does not exist, don't stop
	rm -f *.o
	rm -f $(LIB_DIR)/libBirdeeRuntime.a
remake: clean $(LIB_DIR)/libBirdeeRuntime.a
