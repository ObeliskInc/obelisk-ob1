CC = gcc -std=c11
CFLAGS = -ggdb -s -Os -Wformat=2 -Werror -Wall -Wextra -Wswitch-default -Wswitch-enum -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn -Wno-unused-variable

DEPS = blake256.h dcrverify.h

all: dirs bin/test

dirs:
	mkdir -p bin obj

obj/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

bin/test: obj/blake256.o obj/dcrverify.o obj/test.o
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY:	all dirs test bin/test
