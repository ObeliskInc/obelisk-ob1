clean:
	@rm -rf buildroot

dependencies:
	git clone git://github.com/buildroot/buildroot.git
	cd buildroot && git checkout 2018.02
