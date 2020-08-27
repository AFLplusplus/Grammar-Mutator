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

export ENABLE_DEBUG

.PHONY: all
all: antlr4_shim

.PHONY: antlr4_shim
antlr4_shim:
	@$(MAKE) -C antlr4_shim all

.PHONY: clean
clean:
	@$(MAKE) -C antlr4_shim $@
