#!/bin/bash
#
# This script will replace the apiserver, cgminer, and the webclient on the
# remote control card with the provided upgraded binaries. This script can be
# trivially extended to replace other resources as well or perform other actions
# on the remote control card to get it to a properly upgraded state.
#
# NOTE: You will need the environment variable OB1_PASSWORD set to the password
# of the remote machine, and you will need the environment variable
# OB1_NETADDRESS to the network address of the remote machine.

: ${OB1_NETADDRESS?"Need to set a remote address for the remote control card."}
: ${OB1_PASSWORD?"Need to set a password for the remote control card."}
: ${APISERVER_PATH?"Need to set a path to the upgraded apiserver."}
: ${CGMINER_PATH?"Need to set a path to the upgraded cgminer."}
: ${WEBCLIENT_PATH?"Need to set a path to the upgraded webclient."}

# Set the LEDs to alternating to indicate work in progress.
echo "Initiating control card upgrade"
sshpass -p ${OB1_PASSWORD} ssh root@${OB1_NETADDRESS} << !
	/usr/sbin/led_alternate &
	exit
!

# Copy over a new version of cgminer, apiserver, and of the webclient.
echo "Beginning transfer of binaries."
# sshpass -p ${OB1_PASSWORD} scp ${APISERVER_PATH} root@${OB1_NETADDRESS}:/tmp/apiserver
sshpass -p ${OB1_PASSWORD} scp ${CGMINER_PATH} root@${OB1_NETADDRESS}:/tmp/cgminer
# sshpass -p ${OB1_PASSWORD} scp ${WEBCLIENT_PATH} root@${OB1_NETADDRESS}:/tmp/build/
sshpass -p ${OB1_PASSWORD} ssh root@${OB1_NETADDRESS} << !
	# cp /tmp/apiserver /usr/sbin/apiserver
	# rm /tmp/apiserver
	cp /tmp/cgminer /usr/sbin/cgminer
	rm /tmp/cgminer
	# cp -R /tmp/build /var/www/build
	# rm -r /tmp/build
	exit
!

# Set the LEDs to flasing green to indicate success
echo "Setting LED blink pattern to green"
# For some reason calling 'pidof' remotely doesn't work, so instead we create a
# kill pidof script that we run remotely.
printf "#!/bin/sh\nkill \$(pidof led_alternate)\n" > killcmd.sh
chmod +x killcmd.sh
sshpass -p ${OB1_PASSWORD} scp killcmd.sh root@${OB1_NETADDRESS}:/tmp/killcmd.sh
sshpass -p ${OB1_PASSWORD} ssh root@${OB1_NETADDRESS} << !
	/tmp/killcmd.sh
	/usr/sbin/led_flash_green &
	rm /tmp/killcmd.sh
	exit
!
rm killcmd.sh

# Upgrade complete. Recovery partitions should now be altered.
