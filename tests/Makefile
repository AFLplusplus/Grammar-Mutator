export ENABLE_DEBUG

BUILD = no
ifeq "$(filter $(MAKECMDGOALS),build)" "build"
override BUILD = yes
endif

ifeq ($(BUILD),yes)

# Check GRAMMAR_FILENAME
ifndef GRAMMAR_FILENAME
$(error Please specify the grammar filename)
endif
ifeq (,$(GRAMMAR_FILENAME))
$(error Please specify the grammar filename)
endif

endif

SRC_FILES = $(wildcard test_*.cpp)
TARGETS = $(SRC_FILES:.cpp=)
MEMCHECK_TARGETS = $(TARGETS:=_memcheck)
OBJS = $(SRC_FILES:.cpp=.o)

GRAMMAR_MUTATOR_LIB = $(realpath ../src/libgrammarmutator-$(GRAMMAR_FILENAME).so)
RXI_MAP_LIB = $(realpath ../third_party/rxi_map/librxi_map.a)

GTEST_DIR = googletest-download
GTEST_VERSION = 1.10.0

VALGRIND = valgrind
VALGRIND_LOG_DIR = valgrind_logs
VALGRIND_CMD = $(VALGRIND) -q --tool=memcheck --leak-check=full --show-leak-kinds=all

GTEST_LIBS = $(GTEST_DIR)/libgtest_main.a $(GTEST_DIR)/libgtest.a
GTEST_INCLUDE = $(GTEST_DIR)/googletest-release-$(GTEST_VERSION)/googletest/include

CXX_FLAGS = -std=gnu++14 $(CXX_FLAGS_OPT)
CXX_DEFINES =
CXX_INCLUDES = -I../include -isystem $(GTEST_DIR)/googletest-release-$(GTEST_VERSION)/googletest/include -isystem $(GTEST_DIR)/googletest-release-$(GTEST_VERSION)/googletest

LIBS = $(GTEST_LIBS) $(GRAMMAR_MUTATOR_LIB)
LDFLAGS = $(LIBS) -lpthread

ifdef ENABLE_DEBUG
CXX_FLAGS += -g -O0
CXX_DEFINES += -DDEBUG_BUILD
else
CXX_FLAGS += -O3
endif

.PHONY: all
all: run_test

.PHONY: build
build: $(TARGETS)
ifneq "$(shell command -v $(AFL_FUZZ) 2>/dev/null)" ""
build: $(JSON_PARSER_DIR)/test_json
endif

.PHONY: check_test_programs
check_test_programs:
	@echo "Check test programs"
	@for target in $(TARGETS); do \
		if [ ! -f $$target ] ; then \
			echo "Error: Please compile test programs at first (make ENABLE_TESTING=1)."; exit 2; \
		fi; \
	done

export GTEST_VERSION

$(GTEST_DIR)/googletest-release-$(GTEST_VERSION):
	@$(MAKE) -C $(GTEST_DIR) all

$(GTEST_LIBS) $(GTEST_INCLUDE): $(GTEST_DIR)/googletest-release-$(GTEST_VERSION)

run_test_%: test_%
	@echo "Test: $<"
	@./$<

test_%_memcheck: test_%
	@echo "Test: $<"
	@$(VALGRIND_CMD) --log-file=$(VALGRIND_LOG_DIR)/$<.log ./$<

.PRECIOUS: test_%
test_%: test_%.o $(LIBS)
	$(CXX) $(CXX_FLAGS) $< -o $@ -Wl,-rpath,$(realpath ../src) $(LDFLAGS)

.PRECIOUS: test_tree_mutation
test_tree_mutation: test_tree_mutation.o $(LIBS) $(RXI_MAP_LIB)
	$(CXX) $(CXX_FLAGS) $< -o $@ -Wl,-rpath,$(realpath ../src) $(LDFLAGS) $(RXI_MAP_LIB)

.PRECIOUS: test_chunk_store
test_chunk_store: test_chunk_store.o $(LIBS) $(RXI_MAP_LIB)
	$(CXX) $(CXX_FLAGS) $< -o $@ -Wl,-rpath,$(realpath ../src) $(LDFLAGS) $(RXI_MAP_LIB)

.PRECIOUS: test_rxi_map
test_rxi_map: test_rxi_map.o $(LIBS) $(RXI_MAP_LIB)
	$(CXX) $(CXX_FLAGS) $< -o $@ -Wl,-rpath,$(realpath ../src) $(LDFLAGS) $(RXI_MAP_LIB)

.PRECIOUS: test_%.o
test_%.o: test_%.cpp $(GTEST_INCLUDE)
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o $@ -c $<

ifeq "$(realpath $(GRAMMAR_FILE))" "$(realpath ../grammars/ruby_grammar.json)"
test_tree.o: test_tree.cpp $(GTEST_INCLUDE)
	$(CXX) -DENABLE_PARSING_ARRAY_RB -DARRAY_RB_PATH=\"`realpath testcases/array.rb`\" \
	       $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o $@ -c $<
endif

.PRECIOUS: test_tree_mutation.o
test_tree_mutation.o: test_tree_mutation.cpp $(GTEST_INCLUDE)
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) -I../third_party/rxi_map $(CXX_FLAGS) -o $@ -c $<

.PRECIOUS: test_chunk_store.o
test_chunk_store.o: test_chunk_store.cpp $(GTEST_INCLUDE)
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) -I../third_party/rxi_map $(CXX_FLAGS) -o $@ -c $<

.PRECIOUS: test_rxi_map.o
test_rxi_map.o: test_rxi_map.cpp $(GTEST_INCLUDE)
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) -I../third_party/rxi_map $(CXX_FLAGS) -o $@ -c $<

.PHONY: run_test
run_test: check_test_programs $(addprefix run_,$(TARGETS))

$(VALGRIND_LOG_DIR):
	@mkdir -p $@

.PHONY: memcheck
memcheck: check_test_programs $(VALGRIND_LOG_DIR) $(MEMCHECK_TARGETS)
	@find $(VALGRIND_LOG_DIR) -name "*.log" -not -empty -exec cat {} + | grep . || exit 0 && exit 2

.PHONY: clean
clean:
	@$(MAKE) -C $(GTEST_DIR) clean
	@rm -f $(TARGETS)
	@rm -f $(OBJS)
	@rm -rf $(VALGRIND_LOG_DIR)
	@rm -rf afl_test_fuzz_out
