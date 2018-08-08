CC = gcc
CFLAGS = -ggdb -s -Os -Wformat=2 -Werror -Wall -Wextra -Wswitch-default -Wswitch-enum -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn

DEPS = blake2-impl.h blake2.h siaverify.h stratum.h

all: dirs bin/test

dirs:
	mkdir -p bin obj

obj/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

bin/test: obj/blake2b-ref.o obj/siaverify.o obj/stratum.o obj/test.o
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY:	all dirs test bin/test
