export ENABLE_DEBUG

.PHONY: all
all: antlr4_shim

.PHONY: antlr4_shim
antlr4_shim:
	@$(MAKE) -C antlr4_shim all

.PHONY: clean
clean:
	@$(MAKE) -C antlr4_shim $@
