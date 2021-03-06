# Makefile for Obelisk SC1 Miner Application and associated hardware support API layers and drivers

TOOLCHAIN_PATH = $(OBELISK_OB1_DIR)/controlCardImage/host/bin
CC = $(TOOLCHAIN_PATH)/arm-linux-gcc
CPP = $(TOOLCHAIN_PATH)/arm-linux-cpp
CFLAGS = -ggdb -s -Os -Wformat=2 -Werror -Wall -Wextra -Wswitch-default -Wswitch-enum -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn

DEPS = gpio.h led_control.h

all: gpio_init led_flash_green led_flash_red led_alternate led_off

gpio_init: bin/gpio_init
led_flash_green: bin/led_flash_green
led_flash_red: bin/led_flash_red
led_alternate: bin/led_alternate
led_off: bin/led_off

obj/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

bin/led_off: obj/gpio.o obj/led_control.o obj/led_off.o
	$(CC) -o $@ $^ $(CFLAGS)

bin/led_alternate: obj/gpio.o obj/led_control.o obj/led_alternate.o
	$(CC) -o $@ $^ $(CFLAGS)

bin/led_flash_green: obj/gpio.o obj/led_control.o obj/led_flash_green.o
	$(CC) -o $@ $^ $(CFLAGS)

bin/led_flash_red: obj/gpio.o obj/led_control.o obj/led_flash_red.o
	$(CC) -o $@ $^ $(CFLAGS)

bin/gpio_init: obj/gpio.o obj/gpio_init.o
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY:	all led_off led_alternate led_flash_green led_flash_red gpio_init
