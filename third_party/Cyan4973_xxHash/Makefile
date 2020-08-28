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

TARGET = libxxhash.a

C_SRC_FILES = $(wildcard *.c)
OBJS = $(C_SRC_FILES:.c=.o)

C_FLAGS = -fPIC $(C_FLAGS_OPT)
C_DEFINES =
C_INCLUDES =

ifdef ENABLE_DEBUG
C_FLAGS += -g -O0
C_DEFINES += -DDEBUG_BUILD
else
C_FLAGS += -O3
endif

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) qc $@ $^
	ranlib $@

%.o: %.c
	$(CC) $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o $@ -c $<

.PHONY: clean
clean:
	@rm -f $(OBJS)
	@rm -f $(TARGET)
