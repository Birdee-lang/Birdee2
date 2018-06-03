.PHONY:all clean directories

MKDIR_P = mkdir -p

##
PWD_DIR=$(shell pwd)
COMPILER_DIR=$(PWD_DIR)/Birdee
INC_DIR=$(PWD_DIR)/Birdee/include
BIN_DIR=$(PWD_DIR)/bin

CXX ?= g++
CPPFLAGS ?= -std=c++14 -g -I$(INC_DIR) -ffast-math -march=native
LIBS ?= -pthread

##
export PWD_DIR CXX CPPFLAGS LIBS COMPILER_DIR INC_DIR BIN_DIR

##
all: directories compiler

directories: ${BIN_DIR}

${BIN_DIR}:
	${MKDIR_P} ${BIN_DIR}

compiler:
	make -C $(COMPILER_DIR)


##
clean:
	make -C $(COMPILER_DIR) clean
	rm -rf ${BIN_DIR}

