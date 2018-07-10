all: release

clean:
	@rm -rf buildroot
	@rm -r downloads

dependencies:
	git clone git://github.com/buildroot/buildroot.git
	cd buildroot && git checkout 2018.02

release:
	export OBELISK_OB1_DIR=$(shell pwd)
	cd controlCardImage && make O=$(shell pwd)/controlCardImage BR2_EXTERNAL=$(shell pwd)/controlCardImage -C ../buildroot sama5d2_som_minimal_defconfig
	cd sdCardImage && make O=$(shell pwd)/sdCardImage BR2_EXTERNAL=$(shell pwd)/sdCardImage -C ../buildroot sama5d2_som_minimal_defconfig

# modify the config files for the control card by running 'make menuconfig'.
# After making modifications, the resulting .config file needs to be copied to
# configs/sama5d2_som_minimal_defconfig if the changes are to be persisted.
control-menu:
	export OBELISK_OB1_DIR=$(shell pwd)
	cd controlCardImage && make O=$(shell pwd)/controlCardImage BR2_EXTERNAL=$(shell pwd)/controlCardImage -C ../buildroot sama5d2_som_minimal_defconfig && \
		make menuconfig

# modify the config files for the sd card by running 'make menuconfig'. After
# making modifications, the resulting .config file needs to be copied to
# configs/sama5d2_som_minimal_defconfig if the changes are to be persisted.
sd-menu:
	export OBELISK_OB1_DIR=$(shell pwd)
	cd sdCardImage && make O=$(shell pwd)/sdCardImage BR2_EXTERNAL=$(shell pwd)/sdCardImage -C ../buildroot sama5d2_som_minimal_defconfig && \
		make menuconfig
