
.PHONY:all clean 

MKDIR_P = mkdir -p
#TESTS_DIR = .
#BLIB_DIR = ../BirdeeHome/blib
TEST_BIN_DIR=${TESTS_DIR}/linuxbin

all:${TEST_BIN_DIR} autoclose_test.test array_literal.test class_inherit_import.test container_test.test concurrent_test.ttest defer_test.test exception_test.test file_test.test \
    functype_test.test gc_test.test hardware_exception_test.test import_test.test logic_obj_cmp.test net_test.ttest operators.test \
	rtti2.test template_link_test.test threading_test.ttest typedptr_test.test vector_test.test reflection_test.test unary_op_test.test \
	ast_write_test.pytest scripttest.pytest template_vararg.pytest getset_test.pytest \
	py_module_test.module



${TEST_BIN_DIR}:
	${MKDIR_P} ${TEST_BIN_DIR}

%.ttest:
	python3 $(BLIB_DIR)/../pylib/bbuild.py -i ${TESTS_DIR} -o $(TEST_BIN_DIR) -lc "-pthread" -le $(TEST_BIN_DIR)/$*.out $*
	$(TEST_BIN_DIR)/$*.out

%.test:
	python3 $(BLIB_DIR)/../pylib/bbuild.py -i ${TESTS_DIR} -o $(TEST_BIN_DIR) -le $(TEST_BIN_DIR)/$*.out $*
	$(TEST_BIN_DIR)/$*.out

%.pytest:
	$(BIN_DIR)/birdeec -s -i $*.py -o bin/$*.o

%.module:
	python3 $(BLIB_DIR)/../pylib/bbuild.py -i ${TESTS_DIR} -o $(TEST_BIN_DIR)  $*

clean:
	find ${TESTS_DIR} -iregex '.*\.\(o\|out\|bmm\)' -delete