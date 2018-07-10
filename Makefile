all: release

clean:
	@rm -rf buildroot
	@rm -r downloads

dependencies:
	git clone git://github.com/buildroot/buildroot.git
	cd buildroot && git checkout 2018.02

release:
	cd controlCardImage && make O=$(pwd) BR2_EXTERNAL=$(pwd) -C ../buildroot sama5d2_som_minimal_defconfig
