TARGET = libantlr4_shim.a

ifdef G4_BUILD
# A .g4 build: the matcher, the parse tables (g4_grammar.cpp), the ANTLR lexer
# and parser, and the base classes from G4_SUPPORT_DIR. All except the shim and
# the matcher are in generated/.
ANTLR4_GENERATED_SRC_FILES = $(wildcard generated/*.cpp)

ANTLR4_SHIM_SRC_FILES = \
	g4_shim.cpp \
	g4_matcher.cpp \
	g4_matcher_antlr.cpp \
	$(ANTLR4_GENERATED_SRC_FILES)
else
ANTLR4_GENERATED_SRC_FILES = \
	generated/GrammarBaseListener.cpp \
	generated/GrammarLexer.cpp \
	generated/GrammarListener.cpp \
	generated/GrammarParser.cpp

ANTLR4_SHIM_SRC_FILES = \
	antlr4_shim.cpp \
	$(ANTLR4_GENERATED_SRC_FILES)
endif

ANTLR4_SHIM_OBJS = $(ANTLR4_SHIM_SRC_FILES:.cpp=.o)

ANTLR4_CXX_RUNTIME_LIB = $(realpath ../../third_party/antlr4-cpp-runtime/libantlr4-runtime.a)
ANTLR4_CXX_RUNTIME_DIR = $(realpath ../../third_party/antlr4-cpp-runtime/antlr4-cpp-runtime-src)

# "-Wno-attributes" is only for generated codes, we cannot do more about it
CXX_FLAGS = -std=gnu++17 -fPIC -Wno-attributes $(CXX_FLAGS_OPT)
# Each compile also writes a .d file with the headers of the object. So a
# change in a header makes the objects that include it again.
DEP_FLAGS = -MMD -MP
CXX_DEFINES =
CXX_INCLUDES = \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src \
	-I../../include \
	-I. \
	-I./generated

LIBS = $(ANTLR4_CXX_RUNTIME_LIB)
LDFLAGS = $(LIBS)

ifdef ENABLE_DEBUG
CXX_FLAGS += -g -O0
CXX_DEFINES += -DDEBUG_BUILD
else
CXX_FLAGS += -O3
endif

.PHONY: all
all: $(TARGET)

# "ar q" adds to an archive that exists. Remove the old archive first, so it
# does not keep old copies of the objects.
$(TARGET): $(ANTLR4_SHIM_OBJS)
	rm -f $@
	$(AR) qc $@ $^
	ranlib $@

ifdef G4_BUILD
# g4_compile.sh prints a hint about G4_SUPPORT_DIR if the compile fails. It
# prints the hint once and makes the folder G4_HINT_MARK. Each build removes
# that folder first. This rule comes before "%.o: %.cpp", because make 3.81
# uses the first rule that matches.
G4_HINT_MARK = generated/.g4_compile_hint

.PHONY: g4-hint-reset
g4-hint-reset:
	@rm -rf $(G4_HINT_MARK)

generated/%.o: generated/%.cpp | g4-hint-reset
	sh g4_compile.sh $(G4_HINT_MARK) $(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) $(DEP_FLAGS) -o $@ -c $<
else
generated/GrammarParser.o: generated/GrammarParser.cpp
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) $(DEP_FLAGS) -Wno-unused-parameter -o $@ -c $<
endif

%.o: %.cpp
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) $(DEP_FLAGS) -o $@ -c $<

-include $(ANTLR4_SHIM_OBJS:.o=.d)

.PHONY: clean
clean:
	@rm -rf generated
	@rm -f *.o *.d
	@rm -f $(TARGET)
