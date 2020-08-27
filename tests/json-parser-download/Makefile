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

TARGET = test_json

JSON_PARSER_VERSION = master
JSON_PARSER_ZIP = $(JSON_PARSER_VERSION).zip
JSON_PARSER_DOWNLOAD_URL = https://github.com/h1994st/json-parser/archive/$(JSON_PARSER_ZIP)
JSON_PARSER_DIR = json-parser-$(JSON_PARSER_VERSION)

JSON_PARSER_SRC_FILES = $(wildcard $(JSON_PARSER_DIR)/src/*.cc)

CC = afl-clang
CXX = afl-clang++
C_FLAGS = -Wno-switch
C_INCLUDES = -I./$(JSON_PARSER_DIR)

LIBS = $(JSON_PARSER_DIR)/libjsonparser.a
LDFLAGS = $(LIBS) -lm

ifdef ENABLE_DEBUG
C_FLAGS += -g -O0
else
C_FLAGS += -O3
endif

.PHONY: all
all: $(TARGET)

$(JSON_PARSER_ZIP):
	@wget -q $(JSON_PARSER_DOWNLOAD_URL) > /dev/null

$(JSON_PARSER_DIR): | $(JSON_PARSER_ZIP)
	@unzip $(JSON_PARSER_ZIP) > /dev/null

$(JSON_PARSER_DIR)/examples/test_json.c: | $(JSON_PARSER_DIR)
$(JSON_PARSER_DIR)/libjsonparser.a: | $(JSON_PARSER_DIR)
	@cd $(JSON_PARSER_DIR) && CC=$(CC) CXX=$(CXX) ./configure > /dev/null && $(MAKE) > /dev/null

$(TARGET): $(JSON_PARSER_DIR)/examples/test_json.c $(LIBS)
	$(CC) $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) $< -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	@rm -f $(JSON_PARSER_ZIP)
	@rm -rf $(JSON_PARSER_DIR)
	@rm -f $(TARGET)
