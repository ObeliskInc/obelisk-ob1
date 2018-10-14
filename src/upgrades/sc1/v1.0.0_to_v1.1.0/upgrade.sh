#!/bin/sh

#if [ -d /tmp/upgrade ]; then
#UPGRADE_PATH=/tmp/upgrade
#fi

#if [ -d /root/upgrade ]; then
UPGRADE_PATH=/root/upgrade
#fi

# Stop everything here again, because the name of the watchdog is wrong in upgraderd.sh
# TODO: The new version of upgraderd.sh will do this for us, so these lines can be removed in
# the next upgrade file.
killall -q watchdog.sh
killall -q cgminer
killall -q apiserver

# Ensure the boards are not hashing during the upgrade and that the fan is still running to cool the chips
# since they were most likely just hashing.  This will also be folded into upgraderd.sh in the version
# that this upgrade installs.
$UPGRADE_PATH/rootfs-overlay/usr/sbin/stopasics

# Remove the old GUI files
rm -rf /var/www

# Remove the old watchdog
rm -f /etc/init.d/S25watchdogd

# Copy all the new files
cp -r $UPGRADE_PATH/rootfs-overlay/* /

# Copy the version - also done in upgraderd.sh, but that didn't exist on first week units, so
# we repeat it here.
cp $UPGRADE_PATH/newVersion /root/.version

# Remove the upgrade files
rm -rf $UPGRADE_PATH

# Change the owner of the new files so that lighttpd can read them
chown -R www-data:www-data /var/www

cp /root/upgrade/newVersion /root/.version
