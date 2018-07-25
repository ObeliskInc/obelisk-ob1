#!/bin/sh

# Script to make the part1.img and part2.img files for copy to the RFS for the SD card build

# Create two xff filled file images matching the MTD partition sizes
`dd if=/dev/zero ibs=64k count=48 | tr "\000" "\377" > controlCardImage/images/imgPart1.img`
`dd if=/dev/zero ibs=64k count=462 | tr "\000" "\377" > controlCardImage/images/imgPart2.img`

# Copy in the bootloader to the first 64k section of part1.img
`dd if=controlCardImage/images/at91bootstrap.bin of=controlCardImage/images/imgPart1.img bs=64k count=1 conv=notrunc`

# Copy in the device tree blob to the second 64k section of part1.img
`dd if=controlCardImage/images/at91-sama5d27_som1_ek.dtb of=controlCardImage/images/imgPart1.img bs=64k count=1 conv=notrunc seek=1`

# Copy in the kernel after the second 64k section of part1.img
`dd if=controlCardImage/images/zImage of=controlCardImage/images/imgPart1.img bs=64k conv=notrunc seek=2`

# Copy the RFS to part2.img
`dd if=controlCardImage/images/rootfs.jffs2 of=controlCardImage/images/imgPart2.img bs=64k conv=notrunc`

# Get an MD5 checksum file for the parttion files
`md5sum controlCardImage/images/imgPart1.img controlCardImage/images/imgPart2.img > controlCardImage/images/files.md5`
