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

DIRS = rxi_map antlr4-cpp-runtime Cyan4973_xxHash
CLEANDIRS = $(DIRS:%=clean-%)

.PHONY: all
all: $(DIRS)

.PHONY: $(DIRS)
$(DIRS):
	@$(MAKE) -C $@

.PHONY: clean
clean: $(CLEANDIRS)

$(CLEANDIRS):
	@$(MAKE) -C $(@:clean-%=%) clean

