TARGET = libgrammarmutator.so

C_SRC_FILES = $(wildcard *.c)
OBJS = $(C_SRC_FILES:.c=.o)

C_FLAGS = -fPIC
C_DEFINES =
C_INCLUDES = -I../include -I../third_party/rxi_map -I../third_party/barrust_set

RXI_MAP_LIB = $(realpath ../third_party/rxi_map/librxi_map.a)
BARRUST_SET_LIB = $(realpath ../third_party/barrust_set/libbarrust_set.a)
LIBS = $(RXI_MAP_LIB) $(BARRUST_SET_LIB)
LDFLAGS = $(LIBS)

ifdef ENABLE_DEBUG
C_FLAGS += -g -O0
C_DEFINES += -DDEBUG_BUILD
else
C_FLAGS += -O3
endif

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(C_FLAGS) -shared $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o $@ -c $<

.PHONY: clean
clean:
	@rm -f $(OBJS)
	@rm -f $(TARGET)
