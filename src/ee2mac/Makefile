appname := ee2mac

CC := $(TOOLCHAIN_PATH)/bin/arm-linux-gcc
CFLAGS := -Wno-psabi -s -Os
LDLIBS :=

srcfiles := ee2mac.c
objects  := $(patsubst %.c, %.o, $(srcfiles))

all: $(appname)

$(appname): $(objects)
	$(CC) $(CFLAGS) $(LDFLAGS) -o bin/$(appname) $(objects) $(LDLIBS)

clean:
	rm -r bin/*
	rm -f *.o
