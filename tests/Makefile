export ENABLE_DEBUG

SRC_FILES = $(wildcard test_*.cpp)
TARGETS = $(SRC_FILES:.cpp=)
OBJS = $(SRC_FILES:.cpp=.o)

GTEST_DIR = googletest-download
GTEST_VERSION = 1.10.0

CXX_FLAGS = -std=gnu++14
CXX_DEFINES =
CXX_INCLUDES = -I../include -isystem $(GTEST_DIR)/googletest-release-$(GTEST_VERSION)/googletest/include -isystem $(GTEST_DIR)/googletest-release-$(GTEST_VERSION)/googletest

LIBS = $(GTEST_DIR)/libgtest_main.a ../src/libgrammarmutator.so $(GTEST_DIR)/libgtest.a
LDFLAGS = $(LIBS) -lpthread

ifdef ENABLE_DEBUG
CXX_FLAGS += -g -O0
CXX_DEFINES += -DDEBUG_BUILD
else
CXX_FLAGS += -O3
endif

.PHONY: all
all: run_test

export GTEST_VERSION

$(GTEST_DIR)/libgtest.a:
$(GTEST_DIR)/libgtest_main.a:
	$(MAKE) -C $(GTEST_DIR) all

test_%: test_%.o $(LIBS)
	$(CXX) $(CXX_FLAGS) $< -o $@ -Wl,-rpath,/home/h1994st/Developer/Fuzzing/Grammar-Mutator/cmake-build-debug/src $(LDFLAGS)

test_%.o: test_%.cpp
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o $@ -c $<

.PHONY: clean
clean:
	$(MAKE) -C $(GTEST_DIR) clean
	rm -rf $(TARGETS)

.PHONY: run_test
run_test: $(TARGETS)
	@for target in $(TARGETS); do \
		if [ $$target = 'test_custom_mutator' ]; then \
			./$$target 100; \
		else \
			./$$target; \
		fi; \
	done

.PHONY: memcheck
memcheck:
	@echo $@  # TODO
