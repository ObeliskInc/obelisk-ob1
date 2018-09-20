#!/bin/sh

while true; do
        # Check for upgrade with correct version
        if [ ! -f "/tmp/upgrade/targetVersion" ]; then
                sleep 5s
                continue # No upgrade available
        elif [ "$(cat /root/.version)" = "$(cat /tmp/upgrade/newVersion)" ]; then
                sleep 5s
                continue # Already upgraded
        elif [ ! "$(cat /root/.version)" = "$(cat /tmp/upgrade/targetVersion)" ]; then
                sleep 5s
                continue # Incorrect version
        fi

        # Allow some time for all upgrade files to be extracted in case we see targetVersion
        # early on in the extraction.
        sleep 5

        # Kill watchdog, cgminer, and apiserver
        killall -q watchdog.sh
        killall -q cgminer
        killall -q apiserver

        # Ensure ASICs are not hashing during the upgrade
        /usr/sbin/stopasics

        # Run upgrade script
        chmod +x /tmp/upgrade/upgrade.sh
        sh /tmp/upgrade/upgrade.sh
        rc=$?
        if [ ! "$rc" = "0" ]; then
                echo "Upgrade failed with code $rc"
                exit $rc
        fi

        # Set new version and reboot
        cp /tmp/upgrade/newVersion /root/.version
        reboot
done
