
.PHONY:all clean 

MKDIR_P = mkdir -p
#TESTS_DIR = .
#BLIB_DIR = ../BirdeeHome/blib
TEST_BIN_DIR=${TESTS_DIR}/linuxbin

all:${TEST_BIN_DIR} dlltest autoclose_test.test array_literal.test interface_test.test class_inherit_import.test container_test.test concurrent_test.ttest \
    defer_test.test exception_test.test file_test.test functype_test.test gc_test.test hardware_exception_test.test \
    import_test.test json_test.test logic_obj_cmp.test net_test.ttest operators.test \
	rtti2.test string_test.test template_link_test.test threading_test.ttest threadpool_test.ttest threadlocal_test.ttest typedptr_test.test\
	vector_test.test reflection_test.test unary_op_test.test \
	ast_write_test.pytest scripttest.pytest template_vararg.pytest getset_test.pytest \
	py_module_test.module


dlltest:
	python3 $(BLIB_DIR)/../pylib/bbuild.py -i ${TESTS_DIR} -o $(TEST_BIN_DIR) -ls $(TEST_BIN_DIR)/dlltest.so dll_test
	python3 $(BLIB_DIR)/../pylib/bbuild.py -i ${TESTS_DIR} -o $(TEST_BIN_DIR) -ls $(TEST_BIN_DIR)/dlltest2.so dll_test2
	python3 $(BLIB_DIR)/../pylib/bbuild.py -i ${TESTS_DIR} -o $(TEST_BIN_DIR) -lc "$(TEST_BIN_DIR)/dlltest.so $(TEST_BIN_DIR)/dlltest2.so" -le $(TEST_BIN_DIR)/dll_test_loader.out dll_test_loader
	$(TEST_BIN_DIR)/dll_test_loader.out

${TEST_BIN_DIR}:
	${MKDIR_P} ${TEST_BIN_DIR}
	${MKDIR_P} ${TESTS_DIR}/bin

%.ttest:
	python3 $(BLIB_DIR)/../pylib/bbuild.py -i ${TESTS_DIR} -o $(TEST_BIN_DIR) -lc "-pthread" -le $(TEST_BIN_DIR)/$*.out $*
	$(TEST_BIN_DIR)/$*.out

%.test:
	python3 $(BLIB_DIR)/../pylib/bbuild.py -i ${TESTS_DIR} -o $(TEST_BIN_DIR) -le $(TEST_BIN_DIR)/$*.out $*
	$(TEST_BIN_DIR)/$*.out

%.pytest:
	$(BIN_DIR)/birdeec -s -i $*.py -o $(TEST_BIN_DIR)/$*.o

%.module:
	python3 $(BLIB_DIR)/../pylib/bbuild.py -i ${TESTS_DIR} -o $(TEST_BIN_DIR)  $*

clean:
	find ${TESTS_DIR} -iregex '.*\.\(o\|out\|so\|bmm\)' -delete
