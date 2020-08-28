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

ANTLR4_CXX_RUNTIME_ZIP = antlr4-cpp-runtime-4.8-source.zip
ANTLR4_CXX_RUNTIME_DOWNLOAD_URL = https://www.antlr.org/download/$(ANTLR4_CXX_RUNTIME_ZIP)
ANTLR4_CXX_RUNTIME_DIR = antlr4-cpp-runtime-src

ANTLR4_RUNTIME_SRC_CXX_FILES = \
	ANTLRErrorListener.cpp \
	ANTLRErrorStrategy.cpp \
	ANTLRFileStream.cpp \
	ANTLRInputStream.cpp \
	BailErrorStrategy.cpp \
	BaseErrorListener.cpp \
	BufferedTokenStream.cpp \
	CharStream.cpp \
	CommonToken.cpp \
	CommonTokenFactory.cpp \
	CommonTokenStream.cpp \
	ConsoleErrorListener.cpp \
	DefaultErrorStrategy.cpp \
	DiagnosticErrorListener.cpp \
	Exceptions.cpp \
	FailedPredicateException.cpp \
	InputMismatchException.cpp \
	InterpreterRuleContext.cpp \
	IntStream.cpp \
	Lexer.cpp \
	LexerInterpreter.cpp \
	LexerNoViableAltException.cpp \
	ListTokenSource.cpp \
	NoViableAltException.cpp \
	Parser.cpp \
	ParserInterpreter.cpp \
	ParserRuleContext.cpp \
	ProxyErrorListener.cpp \
	RecognitionException.cpp \
	Recognizer.cpp \
	RuleContext.cpp \
	RuleContextWithAltNum.cpp \
	RuntimeMetaData.cpp \
	Token.cpp \
	TokenSource.cpp \
	TokenStream.cpp \
	TokenStreamRewriter.cpp \
	UnbufferedCharStream.cpp \
	UnbufferedTokenStream.cpp \
	Vocabulary.cpp \
	WritableToken.cpp

ANTLR4_RUNTIME_SRC_ATN_CXX_FILES = \
	AbstractPredicateTransition.cpp \
	ActionTransition.cpp \
	AmbiguityInfo.cpp \
	ArrayPredictionContext.cpp \
	ATNConfig.cpp \
	ATNConfigSet.cpp \
	ATN.cpp \
	ATNDeserializationOptions.cpp \
	ATNDeserializer.cpp \
	ATNSerializer.cpp \
	ATNSimulator.cpp \
	ATNState.cpp \
	AtomTransition.cpp \
	BasicBlockStartState.cpp \
	BasicState.cpp \
	BlockEndState.cpp \
	BlockStartState.cpp \
	ContextSensitivityInfo.cpp \
	DecisionEventInfo.cpp \
	DecisionInfo.cpp \
	DecisionState.cpp \
	EmptyPredictionContext.cpp \
	EpsilonTransition.cpp \
	ErrorInfo.cpp \
	LexerAction.cpp \
	LexerActionExecutor.cpp \
	LexerATNConfig.cpp \
	LexerATNSimulator.cpp \
	LexerChannelAction.cpp \
	LexerCustomAction.cpp \
	LexerIndexedCustomAction.cpp \
	LexerModeAction.cpp \
	LexerMoreAction.cpp \
	LexerPopModeAction.cpp \
	LexerPushModeAction.cpp \
	LexerSkipAction.cpp \
	LexerTypeAction.cpp \
	LL1Analyzer.cpp \
	LookaheadEventInfo.cpp \
	LoopEndState.cpp \
	NotSetTransition.cpp \
	OrderedATNConfigSet.cpp \
	ParseInfo.cpp \
	ParserATNSimulator.cpp \
	PlusBlockStartState.cpp \
	PlusLoopbackState.cpp \
	PrecedencePredicateTransition.cpp \
	PredicateEvalInfo.cpp \
	PredicateTransition.cpp \
	PredictionContext.cpp \
	PredictionMode.cpp \
	ProfilingATNSimulator.cpp \
	RangeTransition.cpp \
	RuleStartState.cpp \
	RuleStopState.cpp \
	RuleTransition.cpp \
	SemanticContext.cpp \
	SetTransition.cpp \
	SingletonPredictionContext.cpp \
	StarBlockStartState.cpp \
	StarLoopbackState.cpp \
	StarLoopEntryState.cpp \
	TokensStartState.cpp \
	Transition.cpp \
	WildcardTransition.cpp

ANTLR4_RUNTIME_SRC_DFA_CXX_FILES = \
	DFA.cpp \
	DFASerializer.cpp \
	DFAState.cpp \
	LexerDFASerializer.cpp

ANTLR4_RUNTIME_SRC_MISC_CXX_FILES = \
	InterpreterDataReader.cpp \
	Interval.cpp \
	IntervalSet.cpp \
	MurmurHash.cpp \
	Predicate.cpp

ANTLR4_RUNTIME_SRC_SUPPORT_CXX_FILES = \
	Any.cpp \
	Arrays.cpp \
	CPPUtils.cpp \
	guid.cpp \
	StringUtils.cpp

ANTLR4_RUNTIME_SRC_TREE_CXX_FILES = \
	ErrorNode.cpp \
	ErrorNodeImpl.cpp \
	IterativeParseTreeWalker.cpp \
	ParseTree.cpp \
	ParseTreeListener.cpp \
	ParseTreeVisitor.cpp \
	ParseTreeWalker.cpp \
	TerminalNode.cpp \
	TerminalNodeImpl.cpp \
	Trees.cpp

ANTLR4_RUNTIME_SRC_TREE_PATTERN_CXX_FILES = \
	Chunk.cpp \
	ParseTreeMatch.cpp \
	ParseTreePattern.cpp \
	ParseTreePatternMatcher.cpp \
	RuleTagToken.cpp \
	TagChunk.cpp \
	TextChunk.cpp \
	TokenTagToken.cpp

ANTLR4_RUNTIME_SRC_TREE_XPATH_CXX_FILES = \
	XPath.cpp \
	XPathElement.cpp \
	XPathLexer.cpp \
	XPathLexerErrorListener.cpp \
	XPathRuleAnywhereElement.cpp \
	XPathRuleElement.cpp \
	XPathTokenAnywhereElement.cpp \
	XPathTokenElement.cpp \
	XPathWildcardAnywhereElement.cpp \
	XPathWildcardElement.cpp

ANTLR4_CXX_RUNTIME_SRC_FILES = \
	$(ANTLR4_RUNTIME_SRC_CXX_FILES:%=$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/%) \
	$(ANTLR4_RUNTIME_SRC_ATN_CXX_FILES:%=$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/atn/%) \
	$(ANTLR4_RUNTIME_SRC_DFA_CXX_FILES:%=$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/dfa/%) \
	$(ANTLR4_RUNTIME_SRC_MISC_CXX_FILES:%=$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/misc/%) \
	$(ANTLR4_RUNTIME_SRC_SUPPORT_CXX_FILES:%=$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/support/%) \
	$(ANTLR4_RUNTIME_SRC_TREE_CXX_FILES:%=$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/tree/%) \
	$(ANTLR4_RUNTIME_SRC_TREE_PATTERN_CXX_FILES:%=$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/tree/pattern/%) \
	$(ANTLR4_RUNTIME_SRC_TREE_XPATH_CXX_FILES:%=$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/tree/xpath/%)

ANTLR4_CXX_RUNTIME_OBJS = $(ANTLR4_CXX_RUNTIME_SRC_FILES:.cpp=.o)

# "-Wno-attributes -Wno-overloaded-virtual -Wno-multichar" are for external ANTLR4 runtime codes
CXX_FLAGS = -std=gnu++14 -fPIC -Wno-attributes -Wno-overloaded-virtual -Wno-multichar
CXX_DEFINES =
CXX_INCLUDES = \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/atn \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/dfa \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/misc \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/support \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/tree \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/tree/pattern \
	-I$(ANTLR4_CXX_RUNTIME_DIR)/runtime/src/tree/xpath


ifdef ENABLE_DEBUG
CXX_FLAGS += -g -O0
CXX_DEFINES += -DDEBUG_BUILD
else
CXX_FLAGS += -O3
endif

.PHONY: all
all: $(TARGET)

$(ANTLR4_CXX_RUNTIME_ZIP):
	@wget -q $(ANTLR4_CXX_RUNTIME_DOWNLOAD_URL) > /dev/null

$(ANTLR4_CXX_RUNTIME_DIR): | $(ANTLR4_CXX_RUNTIME_ZIP)
	@unzip $(ANTLR4_CXX_RUNTIME_ZIP) -d $(ANTLR4_CXX_RUNTIME_DIR) > /dev/null

$(ANTLR4_CXX_RUNTIME_SRC_FILES): $(ANTLR4_CXX_RUNTIME_DIR)

$(TARGET): $(ANTLR4_CXX_RUNTIME_OBJS)
	$(AR) qc $@ $^
	ranlib $@

%.o: %.cpp
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o $@ -c $<

.PHONY: clean
clean:
	@rm -f $(ANTLR4_CXX_RUNTIME_ZIP)
	@rm -rf $(ANTLR4_CXX_RUNTIME_DIR)
	@rm -f $(OBJS)
	@rm -f $(TARGET)
