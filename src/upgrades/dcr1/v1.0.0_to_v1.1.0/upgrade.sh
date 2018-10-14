#!/bin/sh

if [ ! -d /root/upgrade/rootfs-overlay ]; then
# Don't do anything if there was no file system overlay found
exit 1
fi

# Stop everything here again, because the name of the watchdog is wrong in upgraderd.sh
# TODO: The new version of upgraderd.sh will do this for us, so these lines can be removed in
# the next upgrade file.
killall -q watchdog.sh
killall -q cgminer
killall -q apiserver

# Ensure the boards are not hashing during the upgrade and that the fan is still running to cool the chips
# since they were most likely just hashing.  This will also be folded into upgraderd.sh in the version
# that this upgrade installs.
/root/upgrade/rootfs-overlay/usr/sbin/stopasics

# Remove the old GUI files
rm -rf /var/www

# Remove the old watchdog
rm -f /etc/init.d/S25watchdogd

# Copy all the new files into place
cp -r /root/upgrade/rootfs-overlay/* /

# Change the owner of the new files so that lighttpd can read them
chown -R www-data:www-data /var/www

cp /root/upgrade/newVersion /root/.version
