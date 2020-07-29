export ENABLE_DEBUG

SRC_FILES = $(wildcard test_*.cpp)
TARGETS = $(SRC_FILES:.cpp=)
MEMCHECK_TARGETS = $(TARGETS:=_memcheck)
OBJS = $(SRC_FILES:.cpp=.o)

GRAMMAR_MUTATOR_LIB = $(realpath ../src/libgrammarmutator.so)

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

CXX_FLAGS = -std=gnu++14
CXX_DEFINES =
CXX_INCLUDES = -I../include -isystem $(GTEST_DIR)/googletest-release-$(GTEST_VERSION)/googletest/include -isystem $(GTEST_DIR)/googletest-release-$(GTEST_VERSION)/googletest

LIBS = $(GTEST_DIR)/libgtest_main.a $(GRAMMAR_MUTATOR_LIB) $(GTEST_DIR)/libgtest.a
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
build: $(TARGETS) $(JSON_PARSER_DIR)/test_json

.PHONY: check_grammarmutator
check_grammarmutator:
	@if [ "$(GRAMMAR_MUTATOR_LIB)x" = "x" ] ; then \
		echo "Error: Please compile the grammar mutator at first."; exit 2; \
	fi; \

.PHONY: check_test_programs
check_test_programs:
	@echo "Check test programs"
	@for target in $(TARGETS) $(JSON_PARSER_DIR)/test_json; do \
		if [ ! -f $$target ] ; then \
			echo "Error: Please compile test programs at first (make ENABLE_TESTING=1)."; exit 2; \
		fi; \
	done

export GTEST_VERSION

$(GTEST_DIR)/libgtest.a:
$(GTEST_DIR)/libgtest_main.a:
	@$(MAKE) -C $(GTEST_DIR) all

run_test_%: test_%
	@echo "Test: $<"
	@if [ $< = 'test_custom_mutator' ] ; then \
		./$< 100; \
	else \
		./$<; \
	fi;

test_%_memcheck: test_%
	@echo "Test: $<"
	@if [ $< = 'test_custom_mutator' ] ; then \
		$(VALGRIND_CMD) --log-file=$(VALGRIND_LOG_DIR)/$<.log ./$< 100; \
	else \
		$(VALGRIND_CMD) --log-file=$(VALGRIND_LOG_DIR)/$<.log ./$<; \
	fi;

.PRECIOUS: test_%
test_%: test_%.o $(LIBS)
	@$(CXX) $(CXX_FLAGS) $< -o $@ -Wl,-rpath,$(realpath ../src) $(LDFLAGS)

.PRECIOUS: test_%.o
test_%.o: test_%.cpp
	@$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o $@ -c $<


$(JSON_PARSER_DIR)/test_json:
	@$(MAKE) -C $(JSON_PARSER_DIR) all

$(AFL_FUZZ_OUT_DIR):
	@mkdir -p $@

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
