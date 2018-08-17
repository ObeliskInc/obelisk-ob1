IMAGEROOT = controlCardImage/board/microchip/sama5d2_som/rootfs-overlay

all: common cgminer-dcr create-image-dcr cgminer-sia create-image-sia

common: initial-build build-patches sd-utils control-utils apiserver webclient
dcr: common cgminer-dcr create-image-dcr
sia: common cgminer-sia create-image-sia

full: clean dependencies configs all

clean: 
	@rm -rf $(IMAGEROOT)/usr/sbin/apiserver
	@rm -rf $(IMAGEROOT)/usr/sbin/cgminer
	@rm -rf $(IMAGEROOT)/usr/sbin/gpio_init
	@rm -rf $(IMAGEROOT)/usr/sbin/led_alternate
	@rm -rf $(IMAGEROOT)/usr/sbin/led_flash_green
	@rm -rf $(IMAGEROOT)/usr/sbin/led_flash_red
	@rm -rf controlCardImage/images
	@rm -rf controlCardImage/target/usr/sbin/cgminer
	@rm -rf controlCardImage/target/usr/share/zoneinfo
	@rm -rf controlCardImage/target/var/www
	@rm -rf images
	@rm -rf sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/root/part1.img
	@rm -rf sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/root/part2.img
	@rm -rf sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/root/part3.img
	@rm -rf sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/root/files.md5
	@rm -rf sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/usr/sbin/flash_check
	@rm -rf sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/usr/sbin/led_alternate
	@rm -rf sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/usr/sbin/led_erase_error
	@rm -rf sdCardImage/images
	@rm -rf src/apiserver/src/*.o
	@rm -rf src/apiserver/src/util/s*.o
	@rm -rf src/apiserver/src/handlers/*.o
	@rm -rf src/apiserver/.depend
	@rm -rf src/cgminer/cgminer
	@rm -rf src/cgminer/confdefs.h
	@rm -rf src/cgminer/config.log
	@rm -rf src/cgminer/conftest.c
	@rm -rf src/cgminer/conftest.err
	@rm -rf src/cgminer/Makefile
	@rm -rf src/cgminer/Makefile.in
	@rm -rf src/cgminer/obelisk-model.h
	@rm -rf src/cgminer/*.o
	@rm -rf src/cgminer/.deps
	@rm -rf src/cgminer/autom4te.cache/
	@rm -rf src/cgminer/ccan/Makefile
	@rm -rf src/cgminer/ccan/*.a
	@rm -rf src/cgminer/ccan/opt/.deps
	@rm -rf src/cgminer/ccan/opt/*.o
	@rm -rf src/cgminer/compat/Makefile
	@rm -rf src/cgminer/compat/jansson-2.9/aclocal.m4
	@rm -rf src/cgminer/compat/jansson-2.9/configure
	@rm -rf src/cgminer/compat/jansson-2.9/jansson_private_config.h
	@rm -rf src/cgminer/compat/jansson-2.9/jansson_private_config.h.in~
	@rm -rf src/cgminer/compat/jansson-2.9/Makefile
	@rm -rf src/cgminer/compat/jansson-2.9/Makefile.in
	@rm -rf src/cgminer/compat/jansson-2.9/autom4te.cache
	@rm -rf src/cgminer/compat/jansson-2.9/config.log
	@rm -rf src/cgminer/compat/jansson-2.9/config.status
	@rm -rf src/cgminer/compat/jansson-2.9/doc/Makefile
	@rm -rf src/cgminer/compat/jansson-2.9/libtool
	@rm -rf src/cgminer/compat/jansson-2.9/src/.deps
	@rm -rf src/cgminer/compat/jansson-2.9/src/.libs
	@rm -rf src/cgminer/compat/jansson-2.9/src/Makefile
	@rm -rf src/cgminer/compat/jansson-2.9/src/*.la
	@rm -rf src/cgminer/compat/jansson-2.9/src/*.lo
	@rm -rf src/cgminer/compat/jansson-2.9/src/*.o
	@rm -rf src/cgminer/compat/jansson-2.9/test/Makefile
	@rm -rf src/cgminer/compat/jansson-2.9/test/bin/Makefile
	@rm -rf src/cgminer/compat/jansson-2.9/test/suites/Makefile
	@rm -rf src/cgminer/compat/jansson-2.9/test/suites/api/Makefile
	@rm -rf src/cgminer/lib/Makefile
	@rm -rf src/cgminer/lib/.deps
	@rm -rf src/cgminer/lib/*.a
	@rm -rf src/cgminer/lib/*.o
	@rm -rf src/cgminer/obelisk/.deps
	@rm -rf src/cgminer/obelisk/.dirstamp
	@rm -rf src/cgminer/obelisk/*.o
	@rm -rf src/cgminer/obelisk/dcrhash/bin/
	@rm -rf src/cgminer/obelisk/dcrhash/obj/
	@rm -rf src/cgminer/obelisk/dcrhash/.deps
	@rm -rf src/cgminer/obelisk/dcrhash/.dirstamp
	@rm -rf src/cgminer/obelisk/dcrhash/*.o
	@rm -rf src/cgminer/obelisk/siahash/bin/
	@rm -rf src/cgminer/obelisk/siahash/obj/
	@rm -rf src/cgminer/obelisk/siahash/.deps
	@rm -rf src/cgminer/obelisk/siahash/.dirstamp
	@rm -rf src/cgminer/obelisk/siahash/*.o
	@rm -rf /src/cgminer/obelisk/persist/.deps/
	@rm -rf /src/cgminer/obelisk/persist/.dirstamp
	@rm -rf src/controlCardUtils/bin
	@rm -rf src/controlCardUtils/obj
	@rm -rf src/sdCardUtils/bin
	@rm -rf src/sdCardUtils/obj

# Delete every file generated by the standard build process.
clean-all: clean
	@rm -rf .br-external.mk
	@rm -rf buildArtifacts
	@rm -rf controlCardImage/.br-external.mk
	@rm -rf controlCardImage/.config
	@rm -rf controlCardImage/.config.old
	@rm -rf controlCardImage/build
	@rm -rf controlCardImage/host
	@rm -rf controlCardImage/staging
	@rm -rf controlCardImage/target
	@rm -rf controlCardImage/Makefile
	@rm -rf buildArtifacts/downloads
	@rm -rf sdCardImage/.br-external.mk
	@rm -rf sdCardImage/.config
	@rm -rf sdCardImage/.config.old
	@rm -rf sdCardImage/build
	@rm -rf sdCardImage/host
	@rm -rf sdCardImage/staging
	@rm -rf sdCardImage/target
	@rm -rf sdCardImage/Makefile

# Fetch and set up any dependencies.
dependencies:
	mkdir -p buildArtifacts
	cd buildArtifacts && git clone git://github.com/buildroot/buildroot.git
	cd buildArtifacts/buildroot && git checkout 2018.02

# Fetch the configs and prepare them for a full build.
configs:
	cd controlCardImage && make O=$(shell pwd)/controlCardImage BR2_EXTERNAL=$(shell pwd)/controlCardImage -C ../buildArtifacts/buildroot sama5d2_som_minimal_defconfig
	cd sdCardImage && make O=$(shell pwd)/sdCardImage BR2_EXTERNAL=$(shell pwd)/sdCardImage -C ../buildArtifacts/buildroot sama5d2_som_minimal_defconfig

# Perform the build. The first time it is run on a machine, it may take several
# hours to complete.
initial-build:
	# Remove target files.
	@rm -rf controlCardImage/target/usr/sbin/cgminer
	@rm -rf controlCardImage/target/usr/share/zoneinfo
	@rm -rf controlCardImage/target/var/www
	# Build the control card image.
	cd controlCardImage && make OBELISK_OB1_DIR=$(shell pwd)
	# Build the sd card image.
	cd sdCardImage && make OBELISK_OB1_DIR=$(shell pwd)

# Manually modify some of the built libraries and re-make.
build-patches:
	# Move custom SPI stuff for the control card and sd card into the right
	# place.
	cp src/buildroot-patches/spi_nor_ids.c controlCardImage/build/at91bootstrap3-v3.8.10/driver/spi_flash
	cp src/buildroot-patches/spi_nor_ids.c sdCardImage/build/at91bootstrap3-v3.8.10/driver/spi_flash
	cp src/buildroot-patches/sama5d27_som1_ek.c sdCardImage/build/at91bootstrap3-v3.8.10/board/sama5d27_som1_ek

sd-utils:
	mkdir -p src/sdCardUtils/bin
	mkdir -p src/sdCardUtils/obj
	cd src/sdCardUtils && make OBELISK_OB1_DIR=$(shell pwd)
	# Move LED manipulation tools into sd card rootfs.
	cp src/sdCardUtils/bin/flash_check sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/usr/sbin/flash_check
	cp src/sdCardUtils/bin/led_alternate sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/usr/sbin/led_alternate
	cp src/sdCardUtils/bin/led_erase_error sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/usr/sbin/led_erase_error

control-utils:
	mkdir -p src/controlCardUtils/bin
	mkdir -p src/controlCardUtils/obj
	cd src/controlCardUtils && make all OBELISK_OB1_DIR=$(shell pwd) # For some reason, just running 'make' is insufficient, need 'make all'
	# Move LED manipulation tools into control card.
	mkdir -p $(IMAGEROOT)/usr/sbin/
	cp src/controlCardUtils/bin/gpio_init $(IMAGEROOT)/usr/sbin/gpio_init
	cp src/controlCardUtils/bin/led_alternate $(IMAGEROOT)/usr/sbin/led_alternate
	cp src/controlCardUtils/bin/led_flash_green $(IMAGEROOT)/usr/sbin/led_flash_green
	cp src/controlCardUtils/bin/led_flash_red $(IMAGEROOT)/usr/sbin/led_flash_red

apiserver:
	# Create the apiserver
	mkdir -p src/apiserver/bin src/apiserver/obj
	cd src/apiserver && make OBELISK_OB1_DIR=$(shell pwd)
	cp src/apiserver/bin/apiserver $(IMAGEROOT)/usr/sbin/

webclient:
	# Create the webclient
	rm -rf src/webclient/build
	rm -rf $(IMAGEROOT)/var/www
	cd src/webclient && yarn
	rm -rf src/webclient/node_modules/@types/*/node_modules
	cd src/webclient && yarn build
	cd src/webclient && ./optimize-build.sh

	# Copy the build dir
	mkdir -p $(IMAGEROOT)/var/www
	cp -R src/webclient/build/* $(IMAGEROOT)/var/www

cgminer-dcr:
	echo "#define MODEL DCR1" > ./src/cgminer/obelisk-model.h
	echo "#define ALGO BLAKE256" >> ./src/cgminer/obelisk-model.h
	TOOLCHAIN_PATH=$(shell pwd)/controlCardImage/host bash -c 'cd src/cgminer && autoreconf -i && automake && ./build_cgminer_arm.sh && make'
	cp src/cgminer/cgminer controlCardImage/board/microchip/sama5d2_som/rootfs-overlay/usr/sbin/cgminer

cgminer-sia:
	echo "#define MODEL SC1" > ./src/cgminer/obelisk-model.h
	echo "#define ALGO BLAKE2B" >> ./src/cgminer/obelisk-model.h
	TOOLCHAIN_PATH=$(shell pwd)/controlCardImage/host bash -c 'cd src/cgminer && autoreconf -i && automake && ./build_cgminer_arm.sh && make'
	cp src/cgminer/cgminer controlCardImage/board/microchip/sama5d2_som/rootfs-overlay/usr/sbin/cgminer

create-image-common:
	# Remove the .stamp_built so the images are rebuilt properly to include all
	# changes.
	rm controlCardImage/build/at91bootstrap3-v3.8.10/.stamp_built
	rm controlCardImage/build/linux-linux4sam_5.8/.stamp_built
	rm sdCardImage/build/at91bootstrap3-v3.8.10/.stamp_built
	rm sdCardImage/build/linux-linux4sam_5.8/.stamp_built
	# Build the control card image.
	cd controlCardImage && make OBELISK_OB1_DIR=$(shell pwd)
	# Prepare the control card image for the sd card.
	./controlCardImage/prepareForSDCard.sh
	# Move the control card image to the sd card rootfs.
	mkdir -p sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/root
	cp controlCardImage/images/part1.img sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/root/part1.img
	cp controlCardImage/images/part2.img sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/root/part2.img
	cp controlCardImage/images/part3.img sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/root/part3.img
	cp controlCardImage/images/files.md5 sdCardImage/board/microchip/sama5d2_som/rootfs-overlay/root/files.md5

create-image-dcr: create-image-common
	# Copy dcr specific config
	mkdir -p $(IMAGEROOT)/root/.cgminer/
	cp -rf ./src/cgminer/config/cgminer.conf.dcr $(IMAGEROOT)/root/.cgminer/cgminer.conf
	cp -rf ./src/cgminer/config/cgminer.conf.dcr $(IMAGEROOT)/root/.cgminer/default_cgminer.conf
	# Build the sd card image.
	cd sdCardImage && make OBELISK_OB1_DIR=$(shell pwd)
	# Copy the sd card image to the images/ folder.
	mkdir -p images/
	cp sdCardImage/images/sdcard.img images/dcrSDCard.img
	cp controlCardImage/images/part2.img images/dcrControlCardZImage.img
	cp controlCardImage/images/part3.img images/dcrControlCardRootFS.img

create-image-sia: create-image-common
	# Copy dcr specific config
	mkdir -p $(IMAGEROOT)/root/.cgminer/
	cp -rf ./src/cgminer/config/cgminer.conf.sc $(IMAGEROOT)/root/.cgminer/cgminer.conf
	cp -rf ./src/cgminer/config/cgminer.conf.sc $(IMAGEROOT)/root/.cgminer/default_cgminer.conf
	# Build the sd card image.
	cd sdCardImage && make OBELISK_OB1_DIR=$(shell pwd)
	# Copy the sd card image to the images/ folder.
	mkdir -p images/
	cp sdCardImage/images/sdcard.img images/siaSDCard.img
	cp controlCardImage/images/part2.img images/siaControlCardZImage.img
	cp controlCardImage/images/part3.img images/siaControlCardRootFS.img

# Modify the config files for the control card by running 'make menuconfig'.
# After making modifications, the resulting .config file needs to be copied to
# configs/sama5d2_som_minimal_defconfig if the changes are to be persisted.
control-menu:
	cd controlCardImage && make O=$(shell pwd)/controlCardImage BR2_EXTERNAL=$(shell pwd)/controlCardImage -C ../buildArtifacts/buildroot sama5d2_som_minimal_defconfig && \
		make menuconfig

# Modify the config files for the sd card by running 'make menuconfig'. After
# making modifications, the resulting .config file needs to be copied to
# configs/sama5d2_som_minimal_defconfig if the changes are to be persisted.
sd-menu:
	cd sdCardImage && make O=$(shell pwd)/sdCardImage BR2_EXTERNAL=$(shell pwd)/sdCardImage -C ../buildArtifacts/buildroot sama5d2_som_minimal_defconfig && \
		make menuconfig

control-utils:
	# Compile the control card image utils.
	mkdir -p src/controlCardUtils/bin
	mkdir -p src/controlCardUtils/obj
	cd src/controlCardUtils && make OBELISK_OB1_DIR=$(shell pwd)

.PHONY: all full clean dependencies configs initial-build build-patches sd-utils control-utils apiserver webclient cgminer create-image control-menu sd-menu control-utils
