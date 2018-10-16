libBirdeeRuntime.a: BirdeeRuntime.o
	ar -crv $(LIB_DIR)/$@ $^ 
.PHONY:clean
clean:
	# If .o does not exist, don't stop
	rm -f *.o
	rm -f $(LIB_DIR)/libBirdeeRuntime.a
remake: clean libBirdeeRuntime.a
