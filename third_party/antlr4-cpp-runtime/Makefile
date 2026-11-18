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

TARGET = libantlr4-runtime.a

# The top GNUmakefile exports ANTLR_VERSION. The ANTLR jar must have the same
# version.
ANTLR_VERSION ?= 4.13.2

ANTLR4_CXX_RUNTIME_ZIP = antlr4-cpp-runtime-$(ANTLR_VERSION)-source.zip
ANTLR4_CXX_RUNTIME_DOWNLOAD_URL = https://www.antlr.org/download/$(ANTLR4_CXX_RUNTIME_ZIP)
ANTLR4_CXX_RUNTIME_DIR = antlr4-cpp-runtime-src
ANTLR4_CXX_RUNTIME_SRC_DIR = $(ANTLR4_CXX_RUNTIME_DIR)/runtime/src

# The source list is known only after the zip file is extracted. The "all"
# target extracts it and then runs make again to build the library.
ANTLR4_CXX_RUNTIME_SRC_FILES = $(shell find $(ANTLR4_CXX_RUNTIME_SRC_DIR) -name '*.cpp' 2>/dev/null)
ANTLR4_CXX_RUNTIME_OBJS = $(ANTLR4_CXX_RUNTIME_SRC_FILES:.cpp=.o)

# "-Wno-attributes -Wno-overloaded-virtual -Wno-multichar" are for external ANTLR4 runtime codes
CXX_FLAGS = -std=gnu++17 -fPIC -Wno-attributes -Wno-overloaded-virtual -Wno-multichar
CXX_DEFINES =
CXX_INCLUDES = -I$(ANTLR4_CXX_RUNTIME_SRC_DIR)

ifdef ENABLE_DEBUG
CXX_FLAGS += -g -O0
CXX_DEFINES += -DDEBUG_BUILD
else
CXX_FLAGS += -O3
endif

.PHONY: all
all: $(ANTLR4_CXX_RUNTIME_DIR)
	@$(MAKE) $(TARGET)

$(ANTLR4_CXX_RUNTIME_ZIP):
	@wget -q $(ANTLR4_CXX_RUNTIME_DOWNLOAD_URL) > /dev/null

$(ANTLR4_CXX_RUNTIME_DIR): | $(ANTLR4_CXX_RUNTIME_ZIP)
	@unzip -q $(ANTLR4_CXX_RUNTIME_ZIP) -d $(ANTLR4_CXX_RUNTIME_DIR)

$(TARGET): $(ANTLR4_CXX_RUNTIME_OBJS)
	$(AR) qc $@ $^
	ranlib $@

%.o: %.cpp
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o $@ -c $<

.PHONY: clean
clean:
	@rm -f antlr4-cpp-runtime-*-source.zip
	@rm -rf $(ANTLR4_CXX_RUNTIME_DIR)
	@rm -f $(TARGET)
