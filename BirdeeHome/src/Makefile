

CUR_DIR=$(shell pwd)
PARENT_DIR=$(shell dirname $(CUR_DIR))
BLIB_DIR=$(PARENT_DIR)/blib
BIN=$(PARENT_DIR)/bin

all: ${BLIB_DIR}/birdee.o ${BLIB_DIR}/list.o ${BLIB_DIR}/hash_map.o

print-%  : ; @echo $* = $($*)



${BIN}:
	rm -f $(PARENT_DIR)/bin
	ln -s $(PARENT_DIR)/../bin ${BIN}

${BLIB_DIR}:
	mkdir -p ${BLIB_DIR}

${BLIB_DIR}/birdee.o: birdee.txt ${BLIB_DIR} ${BIN}
	$(BIN)/birdeec --corelib -i birdee.txt -o $(BLIB_DIR)/birdee.o

${BLIB_DIR}/list.o: list.txt ${BLIB_DIR}/birdee.o
	$(BIN)/birdeec -i list.txt -o $(BLIB_DIR)/list.o

${BLIB_DIR}/hash_map.o: hash_map.txt ${BLIB_DIR}/birdee.o
	$(BIN)/birdeec -i hash_map.txt -o $(BLIB_DIR)/hash_map.o


clean:
	rm -rf ${BLIB_DIR}