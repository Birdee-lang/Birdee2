libBirdeeRuntime.a: BirdeeRuntime.o
	ar -crv $(BIN_DIR)/$@ $^ 
.PHONY:clean
clean:
	# If .o does not exist, don't stop
	rm -f *.o
	rm -f $(BIN_DIR)/libBirdeeRuntime.a
remake: clean libBirdeeRuntime.a
