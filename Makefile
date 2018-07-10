all: release

clean:
	@rm -rf buildroot
	@rm -r downloads

dependencies:
	git clone git://github.com/buildroot/buildroot.git
	cd buildroot && git checkout 2018.02

release:
	cd controlCardImage && make O=$(shell pwd)/controlCardImage BR2_EXTERNAL=$(shell pwd)/controlCardImage -C ../buildroot sama5d2_som_minimal_defconfig
	cd sdCardImage && make O=$(shell pwd)/sdCardImage BR2_EXTERNAL=$(shell pwd)/sdCardImage -C ../buildroot sama5d2_som_minimal_defconfig
