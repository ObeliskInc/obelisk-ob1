# Makefile for Obelisk SC1 Miner Application and associated hardware support API layers and drivers

TOOLCHAIN_PATH = $(OBELISK_OB1_DIR)/controlCardImage/host/bin
CC = $(TOOLCHAIN_PATH)/arm-linux-gcc
CPP = $(TOOLCHAIN_PATH)/arm-linux-cpp
CFLAGS = -ggdb -O0 -Wformat=2 -Werror -Wall -Wextra -Wswitch-default -Wswitch-enum -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn

DEPS = gpio.h led_control.h

all: gpio_init led_flash_green led_alternate

obj/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

led_alternate: obj/gpio.o obj/led_control.o obj/led_alternate.o
	$(CC) -o bin/$@ $^ $(CFLAGS)

led_flash_green: obj/gpio.o obj/led_control.o obj/led_flash_green.o
	$(CC) -o bin/$@ $^ $(CFLAGS)

gpio_init: obj/gpio.o obj/gpio_init.o
	$(CC) -o bin/$@ $^ $(CFLAGS)

.PHONY:	all led_alternate led_flash_green gpio_init
