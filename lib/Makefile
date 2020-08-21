export ENABLE_DEBUG
export ANTLR_JAR_LOCATION

.PHONY: all
all: antlr4_shim

.PHONY: antlr4_shim
antlr4_shim:
	@$(MAKE) -C antlr4_shim all

.PHONY: clean
clean:
	@$(MAKE) -C antlr4_shim $@
