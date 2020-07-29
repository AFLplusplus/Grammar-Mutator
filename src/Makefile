TARGET = libgrammarmutator.so

# C_SRC_FILES = $(wildcard *.c)
C_SRC_FILES = json_c_fuzz.c list.c tree.c tree_mutation.c tree_trimming.c
CXX_SRC_FILES = $(wildcard *.cpp)
OBJS = $(C_SRC_FILES:.c=.o) $(CXX_SRC_FILES:.cpp=.o)

C_FLAGS = -fPIC
C_DEFINES =
C_INCLUDES = -I../include
CXX_FLAGS = -fPIC -std=gnu++14
CXX_DEFINES =
CXX_INCLUDES = -I../include

ifdef ENABLE_DEBUG
C_FLAGS += -g -O0
C_DEFINES += -DDEBUG_BUILD
CXX_FLAGS += -g -O0
CXX_DEFINES += -DDEBUG_BUILD
else
C_FLAGS += -O3
CXX_FLAGS += -O3
endif

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXX_FLAGS) -shared $^ -o $@

%.o: %.c
	$(CC) $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o $@ -c $<

%.o: %.cpp
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o $@ -c $<

.PHONY: clean
clean:
	@rm -f $(OBJS)
	@rm -f $(TARGET)
