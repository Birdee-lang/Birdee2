BLIB_DIR=$(PWD_DIR)/BirdeeHome/bin/blib

all: ${BLIB_DIR}/birdee.o ${BLIB_DIR}/list.o ${BLIB_DIR}/hash_map.o


${BLIB_DIR}:
	mkdir -p ${BLIB_DIR}

test2:test2.o $(BLIB_DIR)/birdee.o
	gcc -o ../bin/test2 test2.o ../bin/blib/birdee.o ../../bin/libBirdeeRuntime.a
test2.o:test2.txt ${BLIB_DIR}
	$(BIN_DIR)/birdeec -i test2.txt -o test2.o -e

${BLIB_DIR}/birdee.o: birdee.txt ${BLIB_DIR}
	$(BIN_DIR)/birdeec --corelib -i birdee.txt -o $(BLIB_DIR)/birdee.o

${BLIB_DIR}/list.o: list.txt ${BLIB_DIR}/birdee.o
	$(BIN_DIR)/birdeec -i list.txt -o $(BLIB_DIR)/list.o

${BLIB_DIR}/hash_map.o: hash_map.txt ${BLIB_DIR}/birdee.o
	$(BIN_DIR)/birdeec -i hash_map.txt -o $(BLIB_DIR)/hash_map.o


clean:
	rm -rf ${BLIB_DIR}