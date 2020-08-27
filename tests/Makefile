export ENABLE_DEBUG

SRC_FILES = $(wildcard test_*.cpp)
TARGETS = $(SRC_FILES:.cpp=)
MEMCHECK_TARGETS = $(TARGETS:=_memcheck)
OBJS = $(SRC_FILES:.cpp=.o)

GRAMMAR_MUTATOR_LIB = $(realpath ../src/libgrammarmutator.so)
RXI_MAP_LIB = $(realpath ../third_party/rxi_map/librxi_map.a)
BARRUST_SET_LIB = $(realpath ../third_party/barrust_set/libbarrust_set.a)

GTEST_DIR = googletest-download
GTEST_VERSION = 1.10.0

JSON_PARSER_DIR = json-parser-download

AFL_FUZZ = afl-fuzz
AFL_FUZZ_OUT_DIR = /tmp/afl_fuzz_out
AFL_FUZZ_ENVS = AFL_SKIP_CPUFREQ=1 \
                AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
                AFL_CUSTOM_MUTATOR_ONLY=1 \
                AFL_CUSTOM_MUTATOR_LIBRARY=$(GRAMMAR_MUTATOR_LIB)

VALGRIND = valgrind
VALGRIND_LOG_DIR = valgrind_logs
VALGRIND_CMD = $(VALGRIND) -q --tool=memcheck --leak-check=full --show-leak-kinds=all

GTEST_LIBS = $(GTEST_DIR)/libgtest_main.a $(GTEST_DIR)/libgtest.a
GTEST_INCLUDE = $(GTEST_DIR)/googletest-release-$(GTEST_VERSION)/googletest/include

CXX_FLAGS = -std=gnu++14
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
all: check_grammarmutator run_test

.PHONY: build
build: $(TARGETS)
ifneq "$(shell command -v $(AFL_FUZZ) 2>/dev/null)" ""
build: $(JSON_PARSER_DIR)/test_json
endif

.PHONY: check_grammarmutator
check_grammarmutator:
	@if [ "$(GRAMMAR_MUTATOR_LIB)x" = "x" ] ; then \
		echo "Error: Please compile the grammar mutator at first."; exit 2; \
	fi; \

.PHONY: check_test_programs
check_test_programs:
	@echo "Check test programs"
	@for target in $(TARGETS); do \
		if [ ! -f $$target ] ; then \
			echo "Error: Please compile test programs at first (make ENABLE_TESTING=1)."; exit 2; \
		fi; \
	done
ifneq "$(shell command -v $(AFL_FUZZ) 2>/dev/null)" ""
	@if [ ! -f $(JSON_PARSER_DIR)/test_json ] ; then \
		echo "Error: Please compile test programs at first (make ENABLE_TESTING=1)."; exit 2; \
	fi;
endif

export GTEST_VERSION

$(GTEST_LIBS) $(GTEST_INCLUDE):
	@$(MAKE) -C $(GTEST_DIR) all

run_test_%: test_%
	@echo "Test: $<"
	@./$<

test_%_memcheck: test_%
	@echo "Test: $<"
	@$(VALGRIND_CMD) --log-file=$(VALGRIND_LOG_DIR)/$<.log ./$<

.PRECIOUS: test_%
test_%: test_%.o $(LIBS)
	$(CXX) $(CXX_FLAGS) $< -o $@ -Wl,-rpath,$(realpath ../src) $(LDFLAGS)

.PRECIOUS: test_chunk_store
test_chunk_store: test_chunk_store.o $(LIBS) $(RXI_MAP_LIB)
	$(CXX) $(CXX_FLAGS) $< -o $@ -Wl,-rpath,$(realpath ../src) $(LDFLAGS) $(RXI_MAP_LIB)

.PRECIOUS: test_rxi_map
test_rxi_map: test_rxi_map.o $(LIBS) $(RXI_MAP_LIB)
	$(CXX) $(CXX_FLAGS) $< -o $@ -Wl,-rpath,$(realpath ../src) $(LDFLAGS) $(RXI_MAP_LIB)

.PRECIOUS: test_barrust_set
test_barrust_set: test_barrust_set.o $(LIBS) $(BARRUST_SET_LIB)
	$(CXX) $(CXX_FLAGS) $< -o $@ -Wl,-rpath,$(realpath ../src) $(LDFLAGS) $(BARRUST_SET_LIB)

.PRECIOUS: test_%.o
test_%.o: test_%.cpp $(GTEST_INCLUDE)
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o $@ -c $<

.PRECIOUS: test_chunk_store.o
test_chunk_store.o: test_chunk_store.cpp $(GTEST_INCLUDE)
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) -I../third_party/rxi_map -I../third_party/barrust_set $(CXX_FLAGS) -o $@ -c $<

.PRECIOUS: test_rxi_map.o
test_rxi_map.o: test_rxi_map.cpp $(GTEST_INCLUDE)
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) -I../third_party/rxi_map $(CXX_FLAGS) -o $@ -c $<

.PRECIOUS: test_barrust_set.o
test_barrust_set.o: test_barrust_set.cpp $(GTEST_INCLUDE)
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) -I../third_party/barrust_set $(CXX_FLAGS) -o $@ -c $<

$(JSON_PARSER_DIR)/test_json:
	@$(MAKE) -C $(JSON_PARSER_DIR) all

$(AFL_FUZZ_OUT_DIR):
	@mkdir -p $@

ifneq "$(realpath $(GRAMMAR_FILE))" "$(realpath ../grammars/json_grammar.json)"

# Not using the default JSON grammar
.PHONY: fuzz_json_parser
fuzz_json_parser:
	@echo "Skip JSON grammar mutator test"

.PHONY: fuzz_json_parser_memcheck
fuzz_json_parser_memcheck:
	@echo "Skip JSON grammar mutator test"

else
ifneq "$(shell command -v $(AFL_FUZZ) 2>/dev/null)" ""

.PHONY: fuzz_json_parser
fuzz_json_parser: $(JSON_PARSER_DIR)/test_json | $(AFL_FUZZ_OUT_DIR)
	@echo "Test: fuzz json_parser"
	@echo "[==========] Running afl-fuzz."
	@$(AFL_FUZZ_ENVS) $(AFL_FUZZ) -V 10 -s 0 -i ../examples/JSON/in -o $(AFL_FUZZ_OUT_DIR) -- $< @@ > /dev/null
	@echo "[  PASSED  ] 1 test."

.PHONY: fuzz_json_parser_memcheck
fuzz_json_parser_memcheck: $(JSON_PARSER_DIR)/test_json | $(AFL_FUZZ_OUT_DIR)
	@echo "Test: fuzz json_parser"
	@echo "[==========] Running afl-fuzz."
	@$(AFL_FUZZ_ENVS) $(VALGRIND_CMD) --log-file=$(VALGRIND_LOG_DIR)/fuzz_json_parser.log $(AFL_FUZZ) -V 10 -s 0 -i ../examples/JSON/in -o $(AFL_FUZZ_OUT_DIR) -- $< @@ > /dev/null
	@echo "[  PASSED  ] 1 test."

else

# No afl-fuzz
.PHONY: fuzz_json_parser
fuzz_json_parser:
	@echo "No $(AFL_FUZZ) command"

.PHONY: fuzz_json_parser_memcheck
fuzz_json_parser_memcheck:
	@echo "No $(AFL_FUZZ) command"

endif
endif

.PHONY: run_test
run_test: check_test_programs $(addprefix run_,$(TARGETS)) fuzz_json_parser

$(VALGRIND_LOG_DIR):
	@mkdir -p $@

.PHONY: memcheck
memcheck: check_test_programs $(VALGRIND_LOG_DIR) $(MEMCHECK_TARGETS) fuzz_json_parser_memcheck
	cat $(VALGRIND_LOG_DIR)/*.log

.PHONY: clean
clean:
	@$(MAKE) -C $(GTEST_DIR) clean
	@$(MAKE) -C $(JSON_PARSER_DIR) clean
	@rm -f $(TARGETS)
	@rm -f $(OBJS)
	@rm -rf $(VALGRIND_LOG_DIR)
