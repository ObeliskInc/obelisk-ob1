#!/bin/sh

# Script to make the part1.img and part2.img files for copy to the RFS for the SD card build
orig=`pwd`
cd controlCardImage/images

# Create three xff filled file images matching the MTD partition sizes
`dd if=/dev/zero ibs=64k count=2 | tr "\000" "\377" > part1.img`
`dd if=/dev/zero ibs=64k count=46 | tr "\000" "\377" > part2.img`
`dd if=/dev/zero ibs=64k count=192 | tr "\000" "\377" > part3.img`

# Copy in the bootloader to the first 64k section of part1.img
`dd if=at91bootstrap.bin of=part1.img bs=64k count=1 conv=notrunc`

# Copy in the device tree blob to the second 64k section of part1.img
`dd if=at91-sama5d27_som1_ek.dtb of=part1.img bs=64k count=1 conv=notrunc seek=1`

# Copy the kernel to part2.img
`dd if=zImage of=part2.img bs=64k conv=notrunc`

# Copy the RFS to part3.img
`dd if=rootfs.jffs2 of=part3.img bs=64k conv=notrunc`

# Get an MD5 checksum file for the parttion files
`md5sum part1.img part2.img part3.img > files.md5`

cd $orig
