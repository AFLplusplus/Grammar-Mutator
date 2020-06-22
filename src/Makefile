AFLPP_ROOT ?= ../../AFLplusplus

all: libjsonmutator.so

libjsonmutator.so:
	$(CC) $(CFLAGS) -D_FORTIFY_SOURCE=2 -O3 -fPIC -shared -g -I $(AFLPP_ROOT)/"include" -I $(AFLPP_ROOT)/"examples/custom_mutators" json_custom_mutator.c json_c_fuzz.c -o libjsonmutator.so

clean:
	rm -rf libjsonmutator.so
