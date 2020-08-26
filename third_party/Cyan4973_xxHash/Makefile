TARGET = libxxhash.a

C_SRC_FILES = $(wildcard *.c)
OBJS = $(C_SRC_FILES:.c=.o)

C_FLAGS = -fPIC
C_DEFINES =
C_INCLUDES =

ifdef ENABLE_DEBUG
C_FLAGS += -g -O0
C_DEFINES += -DDEBUG_BUILD
else
C_FLAGS += -O3
endif

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) qc $@ $^
	ranlib $@

%.o: %.c
	$(CC) $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o $@ -c $<

.PHONY: clean
clean:
	@rm -f $(OBJS)
	@rm -f $(TARGET)
