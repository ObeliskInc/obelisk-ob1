#!/bin/bash
#
# This script replaces the zImage and rootfs in the recovery partitions so that
# the next time the user does a factory reset, their system will have been
# upgraded.
#
# NOTE: You will need the environment variable OB1_PASSWORD set to the password
# of the remote machine, and you will need the environment variable
# OB1_NETADDRESS to the network address of the remote machine.

# Copy over the new rootfs, replacing the existing recovery rootfs. Do this
# first since it's larger and less important.
echo "Setting LED blink pattern to altenate"
sshpass -p obelisk ssh root@${OB1_NETADDRESS} << !
	/usr/sbin/led_alternate
	exit
!
echo "Beginning SCP of controlCardRootFS"
sshpass -p ${OB1_PASSWORD} scp images/controlCardRootFS.img root@${OB1_NETADDRESS}:/tmp/newRootFS.img
echo "Beginning DD of controlCardRootFS"
sshpass -p obelisk ssh root@${OB1_NETADDRESS} << !
	dd if=/tmp/newRootFS.img of=/dev/mtdblock4
	rm /tmp/newRootFS.img
	exit
!

# Copy over the new zImage, replacing the existing recovery zImage. Do this
# second as the image is smaller and more important. We have confidence at this
# point that there is space for it in /tmp, as we just deleted a larger file.
echo "Beginning SCP of controlCardZImage"
sshpass -p ${OB1_PASSWORD} scp images/controlCardZImage.img root@${OB1_NETADDRESS}:/tmp/newZImage.img
echo "Beginning DD of controlCardZImage"
sshpass -p obelisk ssh root@${OB1_NETADDRESS} << !
	dd if=/tmp/newZImage.img of=/dev/mtdblock5
	rm /tmp/newZImage.img
	exit
!
echo "Setting LED blink pattern to green"
sshpass -p obelisk ssh root@${OB1_NETADDRESS} << !
	/usr/sbin/led_blink_green
	exit
!

# Upgrade complete. Recovery partitions should now be altered.
