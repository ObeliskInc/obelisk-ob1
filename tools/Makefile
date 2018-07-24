# Makefile for Obelisk SC1 Miner Application and associated hardware support API layers and drivers

VERSION = "1.0.1"

BUILD_HOME = `pwd`
SOURCE_DIR = $(BUILD_HOME)/src
HAL_DIR = $(SOURCE_DIR)/hal
UTIL_DIR = $(SOURCE_DIR)/utils
INCLUDE_DIR = $(BUILD_HOME)/include
OBJECT_DIR = $(BUILD_HOME)/obj

TOOLCHAIN_PATH = /home/rt/uC-buildroot-II/buildroot-builds/control_card_new/host/bin
CC = $(TOOLCHAIN_PATH)/arm-linux-gcc
CPP = $(TOOLCHAIN_PATH)/arm-linux-cpp
CFLAGS = -I$(INCLUDE_DIR) -ggdb -O0 -Wformat=2 -Werror -Wall -Wextra -Wswitch-default -Wswitch-enum -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn
LIBS =

# List of object files
#_OBJ = main.o gpio-hal.o spi-hal.o i2c-hal.o led-hal.o switch-hal.o fan-hal.o hash-hal.o
#OBJECTS = $(patsubst %,$(OBJECT_DIR)/%,$(_OBJ))
_FCOBJ = flash_check.o
FLASH_CHECK_OBJECTS = $(patsubst %,$(OBJECT_DIR)/%,$(_FCOBJ))
_BSPOBJ = gpio_test.o
BSP_OBJECTS = $(patsubst %,$(OBJECT_DIR)/%,$(_BSPOBJ))
_LEDOBJ = led_alternate.o
LED_OBJECTS = $(patsubst %,$(OBJECT_DIR)/%,$(_LEDOBJ))
_LEDEROBJ = led_erase_error.o
LED_ERASE_OBJECTS = $(patsubst %,$(OBJECT_DIR)/%,$(_LEDEROBJ))
_I2COBJ = i2c_test.o
I2C_OBJECTS = $(patsubst %,$(OBJECT_DIR)/%,$(_I2COBJ))
_SPIOBJ = spi_test.o
SPI_OBJECTS = $(patsubst %,$(OBJECT_DIR)/%,$(_SPIOBJ))
_FANOBJ = fan_speed.o
FAN_OBJECTS = $(patsubst %,$(OBJECT_DIR)/%,$(_FANOBJ))

$(OBJECT_DIR)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

binaries = flash_check gpio_test led_alternate led_erase_error i2c_test spi_test fan_speed

all:	$(binaries)

flash_check:	$(FLASH_CHECK_OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

gpio_test:	$(BSP_OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

led_alternate:	$(LED_OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

led_erase_error:	$(LED_ERASE_OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

i2c_test:	$(I2C_OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

spi_test:	$(SPI_OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

fan_speed:	$(FAN_OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY:	all

clean:
	rm -f $(binaries) $(OBJECT_DIR)/*.o
