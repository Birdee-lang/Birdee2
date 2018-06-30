.PHONY:all clean directories

MKDIR_P = mkdir -p

##
PWD_DIR=$(shell pwd)
COMPILER_DIR=$(PWD_DIR)/Birdee
RUNTIME_DIR=$(PWD_DIR)/BirdeeRuntime
INC_DIR=$(PWD_DIR)/Birdee/include
INC_DIR2=$(PWD_DIR)/dependency/include
BIN_DIR=$(PWD_DIR)/bin

CXX ?= g++
CPPFLAGS ?= -std=c++14 -g -I$(INC_DIR) -I$(INC_DIR2)
LIBS ?= -pthread

##
export PWD_DIR CXX CPPFLAGS LIBS COMPILER_DIR INC_DIR BIN_DIR

##
all: directories compiler runtime

directories: ${BIN_DIR}

${BIN_DIR}:
	${MKDIR_P} ${BIN_DIR}

compiler:
	make -C $(COMPILER_DIR)
	
runtime:
	make -C $(RUNTIME_DIR)

##
clean:
	make -C $(COMPILER_DIR) clean
	make -C $(RUNTIME_DIR) clean
	rm -rf ${BIN_DIR}

