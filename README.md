# OB1 Firmware

The OB1 is the control card for the SC1a and DCR1a units, the first siacoin and
decred miners shipped by Obelisk. This repo contains the files and instructions
needed to build and load firmware onto the OB1 control cards.

The OB1 operating system is a buildroot configuration. We build two different
images using buildroot. The first image is the controlCardImage, which is the
image of the operating system that we actually run on the control card. The
second image is the image of the SD card that we use to put the first image onto
the control card.

The default root password for the OB1 control card is `obelisk`.

## Dependencies Overview

A more detailed breakdown is provided below, but here's a quick checklist for
commonly missing dependencies for debian users:

```
# Updating PATH to include /sbin
export PATH=$PATH:/sbin

# Installing buildroot
sudo apt-get install g++
sudo apt-get install build-essential # debian only

# Installing post-script requirements
sudo apt-get install mtools

# Installing cgminer
sudo apt-get install pkg-config
sudo apt-get install libtool
sudo apt-get install automake

# Installing nodejs and npm
#
# This process is debian specific, if you are running on another system you will
# need to figure out how to get the most recent versions of nodejs and npm on
# your system. Most distros do not have an up-to-date npm and nodejs in the
# package repositories.
sudo apt-get install curl software-properties-common 
curl -sL https://deb.nodesource.com/setup_10.x | sudo bash -
sudo apt-get install nodejs
sudo npm install -g yarn
```

## Buildroot Dependencies

Buildroot generally has very few requirements. Most base configurations need
nothing more than the requirements in the link below. Debian users will often
need to install `g++` and `build-essential`.

https://buildroot.org/downloads/manual/manual.html#requirement

Once you have all of the required dependencies, you should not need root
permissions to build a buildroot system.

## CGMiner Dependencies

Building cgminer will require the debian packages `pkg-config`, `libtool`, and
`automake`.

## Webclient Dependencies

The ob1 board serves an api that can be used to control the miner. The apiserver
binary serves a bunch of web pages which get built by a nodejs stack. To build
the webclient, you will need a recent version of nodejs and npm, and then you
will need yarn. You can get them with the following command:

You will need a recent version of both nodejs and npm. You can get them on
debian with the following commands:

```
sudo apt-get install curl software-properties-common
curl -sL https://deb.nodesource.com/setup_10.x | sudo bash -
sudo apt-get install nodejs
sudo npm install -g yarn
```

## System upgrade dependencies

We have an upgrade script, `upgradeOB1.sh`, which requires the package `sshpass`
to work. The upgarde script is meant to be run from a linux machine. It will run
a script on the control card that replaces binaries with upgraded versions, and
then flashes lights to indicate success or failure.

## Building the Provisioning Firmware

To build everything from scratch, run 'make full'. This operation can take
several hours. Future builds only require calling `make`, which can take a few
minutes but generally should not be a long operation.

When the build is complete, there will be be three files in the images/ folder.
If you are looking to load the firmware onto the control card, the only one you
need is `sdCard.img`. You should copy that onto an SD card using `dd`. Remember
to unmount the SD card before writing the image to it.

```
dd if=images/sdCard.img of=/dev/mmcblk0 BS=4M
```

The SD card is now ready to be inserted into the ob1 control card. After
inserting the SD card and powering the control card on, the LEDs on the control
card should be alternating colors. That means the SD card is writing the
operating system to the contorl card. When it is done, the green LED will start
blinking. This means the machine can be powered off and the SD card can be
removed.

The control card now has a fresh operating system.

## Building the Upgrade Firmware

After running `make`, the images/ folder should contain the files
`controlCardRootFS.img` and `controlCardZImage.img`. These files can be loaded
into the mtdblock4 and mtdblock5 partitions of the control card to prepare for a
firmware update. The script `upgardeOB1.sh` can be used to copy the files into
the right place.

To use `upgradeOB1.sh`, you will to set two environment variables. The first is
the root password of the control card being upgraded, and the second is the IP
address of the control card being upgraded.

```
export OB1_PASSWORD=obelisk
export OB1_NETADDRESS=192.169.1.13
./upgradeOB1.sh
```

This script will replace the recovery paritions of the contorl card with the new
firmware. To complete the upgrade, you will need to perform a firmware reset on
th econtrol card.

A firmware reset can be performed by powering off the machine, holding down the
button, then powering on the machine while the button is still held down. After
3 seconds, the button can be released. The machine will then replace its current
operating system with the operating system in the recovry partition. Because we
have just upgraded our recovery partition, this effectively upgrades the control
caard.

## Support

Questions? Join us in the Obelisk discord at https://discord.gg/zt9JznT
