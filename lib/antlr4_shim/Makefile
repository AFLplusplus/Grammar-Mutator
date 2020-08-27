TARGET = libantlr4_shim.a

ANTLR4_GENERATED_SRC_FILES = \
	generated/GrammarBaseListener.cpp \
	generated/GrammarLexer.cpp \
	generated/GrammarListener.cpp \
	generated/GrammarParser.cpp

ANTLR4_SHIM_SRC_FILES = \
	antlr4_shim.cpp \
	$(ANTLR4_GENERATED_SRC_FILES)

ANTLR4_SHIM_OBJS = $(ANTLR4_SHIM_SRC_FILES:.cpp=.o)

ANTLR4_CXX_RUNTIME_LIB = $(realpath ../../third_party/antlr4-cpp-runtime/libantlr4-runtime.a)
ANTLR4_CXX_RUNTIME_DIR = $(realpath ../../third_party/antlr4-cpp-runtime/antlr4-cpp-runtime-src)

CXX_FLAGS = -std=gnu++14 -fPIC -Wno-attributes
CXX_DEFINES =
CXX_INCLUDES = \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/atn \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/dfa \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/misc \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/support \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/tree \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/tree/pattern \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/tree/xpath \
	-I../../include \
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

generated: ../../grammars/Grammar.g4
	@mkdir -p generated
	java -jar $(ANTLR_JAR_LOCATION) -Dlanguage=Cpp -DcontextSuperClass=antlr4::RuleContextWithAltNum -o generated $(realpath $<)

# Generate lexer and parser
$(ANTLR4_SHIM_SRC_FILES): generated

$(TARGET): $(ANTLR4_SHIM_OBJS)
	$(AR) qc $@ $^
	ranlib $@

%.o: %.cpp
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o $@ -c $<

.PHONY: clean
clean:
	@rm -rf generated
	@rm -f $(ANTLR4_SHIM_OBJS)
	@rm -f $(TARGET)
