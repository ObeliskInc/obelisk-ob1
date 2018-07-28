# Makefile for Obelisk SC1 Miner Application and associated hardware support API layers and drivers

TOOLCHAIN_PATH = $(OBELISK_OB1_DIR)/controlCardImage/host/bin
CC = $(TOOLCHAIN_PATH)/arm-linux-gcc
CPP = $(TOOLCHAIN_PATH)/arm-linux-cpp
CFLAGS = -ggdb -O0 -Wformat=2 -Werror -Wall -Wextra -Wswitch-default -Wswitch-enum -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn

all:
	$(CC) -c -o obj/gpio_setup.o gpio_setup.c $(CFLAGS)
	$(CC) -o bin/gpio_setup obj/gpio_setup.o $(CFLAGS)
	$(CC) -c -o obj/led_control.o led_control.c $(CFLAGS)
	$(CC) -c -o obj/led_alternate.o led_alternate.c $(CFLAGS)
	$(CC) -o bin/led_alternate obj/gpio_setup.o obj/led_control.o obj/led_alternate.o $(CFLAGS)

.PHONY:	all
