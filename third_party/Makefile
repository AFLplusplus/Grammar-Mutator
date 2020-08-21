DIRS = rxi_map barrust_set antlr4-cpp-runtime
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

