import SAMBA 3.2
import SAMBA.Connection.Serial 3.2
import SAMBA.Device.SAMA5D2 3.2

SerialConnection {
	//port: "ttyACM0"
	//port: "COM85"
	//baudRate: 57600

	device: SAMA5D2Xplained {
	//device: SAMA5D2 {
		// to use a custom config, replace SAMA5D2Xplained by SAMA5D2 and
		// uncomment the following lines, or see documentation for
		// custom board creation.
		config {
			qspiflash {
				instance: 1
				ioset: 2
				freq: 66
			}

			sdmmc {
				instance: 0
			}
		}
	}

	onConnectionOpened: {
		// initialize QSPI applet
		//initializeApplet("qspiflash")

		// erase all memory
		//applet.erase(0, applet.memorySize)

		// write files
		//applet.write(0x00000, "images/at91bootstrap.bin", true)
		//applet.write(0x10000, "images/at91-sama5d27_som1_ek.dtb")
		//applet.write(0x20000, "images/zImage")
		//applet.write(0x2a0000, "images/rootfs.jffs2")

		initializeApplet("sdmmc")

		applet.write(0x0, "images/sdcard.img")

		// initialize boot config applet
		initializeApplet("bootconfig")

		// Use BUREG0 as boot configuration word
		applet.writeBootCfg(BootCfg.BSCR, BSCR.fromText("VALID,BUREG0"))

		// Enable external boot only on QSPI1 IOSET2
		applet.writeBootCfg(BootCfg.BUREG0,
			BCW.fromText("EXT_MEM_BOOT,UART1_IOSET1,JTAG_IOSET1," +
			             "SDMMC1,SDMMC0,NFC_DISABLED," +
			             "SPI1_DISABLED,SPI0_DISABLED," +
				     "QSPI1_DISABLED,QSPI0_DISABLED"))
			             //"QSPI1_IOSET2,QSPI0_DISABLED"))
	}
}
