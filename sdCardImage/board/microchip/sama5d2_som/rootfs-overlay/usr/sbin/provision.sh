#!/bin/sh
# TEST Script to provision a new system

#exec 19>/root/provision.log
#BASH_XTRACEFD=19

#set -xv

# Set LEDs to be alternating RED->GREEN->RED
/usr/sbin/led_alternate &

sleep 5

echo `date` > /root/provision.log;
echo "Provisioning memory flash" >> /root/provision.log;
# Check memory segments in MTD to see if they are already blank
/usr/sbin/flash_check /dev/mtd0
if [ $? != 0 ]; then
    echo "MTD0 not empty, erasing" >> /root/provision.log;
    sync
    /usr/sbin/flash_erase /dev/mtd0 0 48
    # Re-test to ensure flash is erased
    /usr/sbin/flash_check /dev/mtd0
    if [ $? != 0 ]; then
        echo "MTD0 erase error" >> /root/provision.log;
        sync
        /usr/sbin/led_erase_error &
        exit 1;
    fi        
else
    echo "MTD0 empty" >> /root/provision.log;
    sync
fi

/usr/sbin/flash_check /dev/mtd1
if [ $? != 0 ]; then
    echo "MTD1 not empty, erasing" >> /root/provision.log;
    sync
    /usr/sbin/flash_erase /dev/mtd1 0 462
    # Re-test to ensure flash is erased
    /usr/sbin/flash_check /dev/mtd1
    if [ $? != 0 ]; then
        echo "MTD1 erase error" >> /root/provision.log;
        sync
        /usr/sbin/led_erase_error &
        exit 1;
    fi
else
    echo "MTD1 empty" >> /root/provision.log;
    sync
fi

/usr/sbin/flash_check /dev/mtd2
if [ $? != 0 ]; then
    echo "MTD2 not empty, erasing" >> /root/provision.log;
    sync
    /usr/sbin/flash_erase /dev/mtd2 0 18
    # Re-test to ensure flash is erased
    /usr/sbin/flash_check /dev/mtd2
    if [ $? != 0 ]; then
        echo "MTD2 erase error" >> /root/provision.log;
        sync
        /usr/sbin/led_erase_error &
        exit 1;
    fi
else
    echo "MTD2 empty" >> /root/provision.log;
    sync
fi

/usr/sbin/flash_check /dev/mtd3
if [ $? != 0 ]; then
    echo "MTD3 not empty, erasing" >> /root/provision.log;
    sync
    /usr/sbin/flash_erase /dev/mtd3 0 190
    # Re-test to ensure flash is erased
    /usr/sbin/flash_check /dev/mtd3
    if [ $? != 0 ]; then
        echo "MTD3 erase error" >> /root/provision.log;
        sync
        /usr/sbin/led_erase_error &
        exit 1;
    fi
else
    echo "MTD3 empty" >> /root/provision.log;
    sync
fi

# Progam the flash memory segments
echo "Writing MTD0" >> /root/provision.log;
sync
dd if=/root/part1.img of=/dev/mtd0 bs=64k
echo "Result: $?" >> /root/provision.log;
echo "Writing MTD1" >> /root/provision.log;
sync
dd if=/root/part2.img of=/dev/mtd1 bs=64k
echo "Result: $?" >> /root/provision.log;
sync

# Verify the flash memory segments
echo "Reading MTD0" >> /root/provision.log;
sync
dd if=/dev/mtd0 of=/tmp/part1.img bs=64k
echo "Reading MTD1" >> /root/provision.log;
sync
dd if=/dev/mtd1 of=/tmp/part2.img bs=64k

echo "Verifying read-back files against MD5 sum file" >> /root/provision.log;
sync
cd /tmp
/usr/bin/md5sum -s -c /root/files.md5

if [ $? != 0 ]; then
    echo "Program of flash did not work" >> /root/provision.log;
    sync
    # Set RED LED solid ON
    /usr/bin/killall led_alternate
    echo 255 > /sys/class/leds/green/brightness;
    echo 0 > /sys/class/leds/red/brightness;
    echo `date` >> /root/provision.log;

    while true
    do
        echo 0 > /sys/class/leds/green/brightness;
        sleep 1;
        echo 255 > /sys/class/leds/green/brightness;
        sleep 1;
    done
else
    echo "Program of flash completed successfully" >> /root/provision.log;
    sync
    # Set GREEN LED solid ON
    /usr/bin/killall led_alternate
    echo 0 > /sys/class/leds/green/brightness;
    echo 255 > /sys/class/leds/red/brightness;
fi

echo `date` >> /root/provision.log;
sync

while true
do
    echo 0 > /sys/class/leds/red/brightness;
    sleep 1;
    echo 255 > /sys/class/leds/red/brightness;
    sleep 1;
done
