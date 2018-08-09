
ACLOCAL_AMFLAGS = -I m4

JANSSON_CPPFLAGS= -I$(top_builddir)/compat/jansson-2.9/src -I$(top_srcdir)/compat/jansson-2.9/src

if WANT_USBUTILS
USBUTILS_CPPFLAGS = `pkg-config libusb-1.0 --libs --cflags`
else
USBUTILS_CPPFLAGS =
endif

if STATIC_CURL
LIBCURL_CPPFLAGS = -DCURL_STATICLIB
else
LIBCURL_CPPFLAGS =
endif

EXTRA_DIST	= example.conf linux-usb-cgminer \
		  api-example.php miner.php	\
		  API.class API.java api-example.c windows-build.txt \
		  bitstreams/README API-README FPGA-README \
		  bitforce-firmware-flash.c hexdump.c ASIC-README \
		  01-cgminer.rules

SUBDIRS		= lib compat ccan

cgminer_CPPFLAGS = $(PTHREAD_FLAGS) -fno-strict-aliasing $(JANSSON_CPPFLAGS) $(USBUTILS_CPPFLAGS) $(LIBCURL_CPPFLAGS)

bin_PROGRAMS	= cgminer

cgminer_LDFLAGS	= $(PTHREAD_FLAGS)
cgminer_LDADD	= $(DLOPEN_FLAGS) @LIBCURL_LIBS@ @JANSSON_LIBS@ @PTHREAD_LIBS@ \
		  @NCURSES_LIBS@ @PDCURSES_LIBS@ @WS2_LIBS@ \
		  @LIBUSB_LIBS@ @MM_LIBS@ @RT_LIBS@ @LIBSYSTEMD_LIBS@ \
		  @MATH_LIBS@ lib/libgnu.a ccan/libccan.a

cgminer_CPPFLAGS += -I$(top_builddir)/lib -I$(top_srcdir)/lib

if !HAVE_WINDOWS
cgminer_CPPFLAGS += @LIBCURL_CFLAGS@
endif

# common sources
cgminer_SOURCES := cgminer.c

cgminer_SOURCES	+= elist.h miner.h compat.h bench_block.h	\
		   util.c util.h uthash.h logging.h		\
		   sha2.c sha2.h api.c

cgminer_SOURCES	+= logging.c

cgminer_SOURCES	+= klist.h klist.c

cgminer_SOURCES	+= noncedup.c

cgminer_SOURCES += -ggdb


if NEED_FPGAUTILS
cgminer_SOURCES += fpgautils.c fpgautils.h
endif

if WANT_USBUTILS
cgminer_SOURCES += usbutils.c usbutils.h
endif

if WANT_LIBBITFURY
cgminer_SOURCES += libbitfury.c libbitfury.h mcp2210.c mcp2210.h
endif

if WANT_CRC16
cgminer_SOURCES += crc16.c crc.h
endif

# Device drivers
if HAS_AVALON
cgminer_SOURCES += driver-avalon.c driver-avalon.h
endif

if HAS_KNC
cgminer_SOURCES += driver-knc.c knc-asic.c knc-asic.h knc-transport.h knc-transport-spi.c
cgminer_LDADD   += -lz
endif

if HAS_BFLSC
cgminer_SOURCES += driver-bflsc.c driver-bflsc.h
endif

if HAS_BITFORCE
cgminer_SOURCES += driver-bitforce.c
endif

if HAS_HASHFAST
cgminer_SOURCES += driver-hashfast.c driver-hashfast.h hf_protocol.h hf_protocol_be.h
endif

if HAS_HASHRATIO
cgminer_SOURCES += driver-hashratio.c driver-hashratio.h
endif

if HAS_BITFURY
cgminer_SOURCES += driver-bitfury.c driver-bitfury.h
endif

if HAS_BITMINE_A1
cgminer_SOURCES += driver-SPI-bitmine-A1.c
cgminer_SOURCES += spi-context.c spi-context.h
cgminer_SOURCES += A1-common.h
cgminer_SOURCES += A1-board-selector.h
cgminer_SOURCES += A1-board-selector-CCD.c A1-board-selector-CCR.c
cgminer_SOURCES += A1-trimpot-mcp4x.h A1-trimpot-mcp4x.c
cgminer_SOURCES += i2c-context.c i2c-context.h
endif

if HAS_DRILLBIT
cgminer_SOURCES += driver-drillbit.c driver-drillbit.h
endif

if HAS_ICARUS
cgminer_SOURCES += driver-icarus.c
endif

if HAS_KLONDIKE
cgminer_SOURCES += driver-klondike.c
endif

if HAS_COINTERRA
cgminer_SOURCES += driver-cointerra.c driver-cointerra.h
endif

if HAS_SP10
cgminer_SOURCES += driver-spondoolies-sp10.c driver-spondoolies-sp10.h \
		   driver-spondoolies-sp10-p.c driver-spondoolies-sp10-p.h
endif


if HAS_SP30
cgminer_SOURCES += driver-spondoolies-sp30.c driver-spondoolies-sp30.h \
		   driver-spondoolies-sp30-p.c driver-spondoolies-sp30-p.h
endif

if HAS_OBELISK
cgminer_SOURCES += driver-obelisk.c hexdump.c obelisk/multicast.c\
			obelisk/Ob1API.c obelisk/Ob1Utils.c obelisk/Ob1API.h obelisk/Ob1Utils.h obelisk/Ob1Defines.h \
			obelisk/Ob1Test.c obelisk/Ob1Utils.h

# Sia hashing/verification code
cgminer_SOURCES += obelisk/siahash/blake2-impl.h obelisk/siahash/blake2.h obelisk/siahash/blake2b-ref.c \
      obelisk/siahash/siaverify.c obelisk/siahash/siaverify.h \
			obelisk/siahash/siastratum.c obelisk/siahash/siastratum.h \
      obelisk/dcrhash/dcrverify.c obelisk/dcrhash/dcrverify.h \
			obelisk/dcrhash/dcrstratum.c obelisk/dcrhash/dcrstratum.h \
			obelisk/dcrhash/blake256.c obelisk/dcrhash/blake256.h

cgminer_SOURCES += obelisk/Usermain.c obelisk/Usermain.h

cgminer_SOURCES += obelisk/gpio_test.c obelisk/gpio_bsp.h

cgminer_SOURCES += obelisk/i2c_test.c obelisk/i2c.h

cgminer_SOURCES += obelisk/spi_test.c obelisk/spi.h

cgminer_SOURCES += obelisk/Ob1Hashboard.c obelisk/Ob1Hashboard.h

cgminer_SOURCES += obelisk/CSS_SC1_hal.c obelisk/CSS_SC1_hal.h obelisk/CSS_SC1Defines.h

cgminer_SOURCES += obelisk/CSS_DCR1_hal.c obelisk/CSS_DCR1_hal.h obelisk/CSS_DCR1Defines.h

cgminer_SOURCES += obelisk/TWI_Support.c sobeliskc1/TWI_Support.h

cgminer_SOURCES += obelisk/SPI_Support.c obelisk/SPI_Support.h

cgminer_SOURCES += obelisk/Console.c obelisk/Console.h

cgminer_SOURCES += obelisk/GenericDefines.h

cgminer_SOURCES += obelisk/MiscSupport.c obelisk/MiscSupport.h

cgminer_SOURCES += obelisk/ads1015.c obelisk/ads1015.h obelisk/ads1015_defines.h

cgminer_SOURCES += obelisk/MCP23S17_hal.c obelisk/MCP23S17_hal.h obelisk/MCP23S17_Defines.h

cgminer_SOURCES += obelisk/MCP24AA02_hal.c obelisk/MCP24AA02_hal.h scobelisk1/MCP24AA02_Defines.h

cgminer_SOURCES += obelisk/MCP4017_hal.c obelisk/MCP4017_hal.h obelisk/MCP4017_defines.h

cgminer_SOURCES += obelisk/MCP9903_hal.c obelisk/MCP9903_hal.h obelisk/MCP9903_defines.h

cgminer_SOURCES += obelisk/TCA9546A_hal.c obelisk/TCA9546A_hal.h obelisk/TCA9546A_defines.h

endif

if HAS_AVALON2
cgminer_SOURCES += driver-avalon2.c driver-avalon2.h
endif

if HAS_AVALON4
cgminer_SOURCES += driver-avalon4.c driver-avalon4.h
endif

if HAS_AVALON7
cgminer_SOURCES += driver-avalon7.c driver-avalon7.h
endif

if NEED_I2C_CONTEXT
cgminer_SOURCES += i2c-context.c
endif

if HAS_AVALON_MINER
cgminer_SOURCES += driver-avalon-miner.c driver-avalon-miner.h
endif

if HAS_MINION
cgminer_SOURCES += driver-minion.c
endif

if HAS_ANT_S1
cgminer_SOURCES += driver-bitmain.c driver-bitmain.h
endif

if HAS_ANT_S2
cgminer_SOURCES += driver-bitmain.c driver-bitmain.h
endif

if HAS_ANT_S3
cgminer_SOURCES += driver-bitmain.c driver-bitmain.h
endif

if HAS_MODMINER
cgminer_SOURCES += driver-modminer.c
bitstreamsdir = $(bindir)/bitstreams
dist_bitstreams_DATA = $(top_srcdir)/bitstreams/README
endif

if HAS_BLOCKERUPTER
cgminer_SOURCES += driver-blockerupter.c driver-blockerupter.h
endif
