#
# american fuzzy lop++ - grammar mutator
# --------------------------------------
#
# Written by Shengtuo Hu
#
# Copyright 2020 AFLplusplus Project. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# A grammar-based custom mutator written for GSoC '20.
#

TARGETS = libgtest.a libgtest_main.a

GTEST_VERSION ?= 1.10.0
GTEST_TAR = release-$(GTEST_VERSION).tar.gz
GTEST_DOWNLOAD_URL = https://github.com/google/googletest/archive/$(GTEST_TAR)
GTEST_ROOT_DIR = googletest-release-$(GTEST_VERSION)
GTEST_DIR = $(GTEST_ROOT_DIR)/googletest
GTEST_SRC_FILES = \
	$(GTEST_DIR)/src/gtest-all.cc \
	$(GTEST_DIR)/src/gtest_main.cc
GTEST_OBJS = $(GTEST_SRC_FILES:.cc=.o)


CXX_FLAGS = -Wall -Wshadow -Werror -Wno-error=dangling-else -DGTEST_HAS_PTHREAD=1 -fexceptions -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -std=c++11
CXX_DEFINES =
CXX_INCLUDES = -I./$(GTEST_DIR)/include -I./$(GTEST_DIR)

ifdef ENABLE_DEBUG
CXX_FLAGS += -g -O0
CXX_DEFINES += -DDEBUG_BUILD
else
CXX_FLAGS += -O3
endif

.PHONY: all
all: $(TARGETS)

$(GTEST_TAR):
	@wget -q $(GTEST_DOWNLOAD_URL) > /dev/null

$(GTEST_ROOT_DIR): | $(GTEST_TAR)
	@tar -xzvf $(GTEST_TAR) > /dev/null

$(GTEST_SRC_FILES): $(GTEST_ROOT_DIR)

libgtest.a: $(GTEST_DIR)/src/gtest-all.o
	$(AR) qc libgtest.a $^
	ranlib libgtest.a

libgtest_main.a: $(GTEST_DIR)/src/gtest_main.o
	$(AR) qc libgtest_main.a $^
	ranlib libgtest_main.a

%.o: %.cc
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o $@ -c $<

.PHONY: clean
clean:
	@rm -f $(GTEST_TAR)
	@rm -rf $(GTEST_ROOT_DIR)
	@rm -f $(GTEST_OBJS)
	@rm -f $(TARGETS)
