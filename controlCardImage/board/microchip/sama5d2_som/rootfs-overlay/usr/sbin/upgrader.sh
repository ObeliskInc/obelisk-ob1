#!/bin/sh

while true; do
	# check for upgrade with correct version
	if [ ! -f "/tmp/targetVersion" ]; then
		sleep 5s
		continue # No upgrade available
	elif [ "$(cat /root/.version)" = "$(cat /tmp/newVersion)" ]; then
		sleep 5s
		continue # Already upgraded
	elif [ ! "$(cat /root/.version)" = "$(cat /tmp/targetVersion)" ]; then
		sleep 5s
		continue # Incorrect version
	fi

	# allow some time for all upgrade files to be copied over
	sleep 5

	# kill watchdog, cgminer, and apiserver
	killall -q watchdog
	killall -q cgminer
	killall -q apiserver

	# run upgrade script
	sh /tmp/upgrade.sh
	rc=$?
	if [ ! "$rc" = "0" ]; then
		echo "Upgrade failed with code $rc"
		exit $rc
	fi

	# set new version and reboot
	cp /tmp/newVersion /root/.version
	reboot
done
