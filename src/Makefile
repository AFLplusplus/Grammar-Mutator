AFLPP_ROOT ?= $(abspath ../../AFLplusplus)

all: libjsonmutator.so

libjsonmutator.so: json_custom_mutator.c json_c_fuzz.c
	$(CC) $(CFLAGS) -D_FORTIFY_SOURCE=2 -fPIC -shared -g -I"$(AFLPP_ROOT)/include" -I../include $^ -o $@

clean:
	rm -rf libjsonmutator.so
