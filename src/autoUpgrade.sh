#!/bin/bash

GRAY='\033[0;37m'
GREEN='\033[0;32m'
LIGHTGREEN='\033[1;32m'

setup_cmd=`cat <<EOF
if [ -e /root/.upgrade_complete ]
then
	exit 93
fi

mkdir -p /tmp/upgrades
/etc/init.d/S25watchdogd stop
killall -q cgminer
nohup /usr/sbin/led_alternate &>/dev/null &
exit
EOF
`

finish_cmd=`cat <<EOF
mv /tmp/upgrades/cgminer /usr/sbin/cgminer
mv /tmp/upgrades/cgminer.conf /root/.cgminer/cgminer.conf
mv /tmp/upgrades/apiserver /usr/sbin/apiserver
rm -rf /var/www/fonts
rm -rf /var/www/static
mv /tmp/upgrades/webclient/* /var/www/
mv /tmp/upgrades/S25watchdogd /etc/init.d/S25watchdogd
rm -rf /tmp/upgrades

./burn-in &>/dev/null
rc=$?
if [ $rc == 0 ]
then
	killall -q led_alternate
	nohup /usr/sbin/led_flash_green &>/dev/null &
	touch /root/.upgrade_complete
elif
	killall -q led_alternate
	nohup /usr/sbin/led_flash_red &>/dev/null &
	exit $rc
fi

exit
EOF
`

# create upgrades dir
rm -rf upgrades
mkdir upgrades
cp cgminer/cgminer upgrades/
cp ../controlCardImage/board/microchip/sama5d2_som/rootfs-overlay/root/.cgminer/cgminer.conf upgrades/
cp apiserver/bin/apiserver upgrades/
cp -R webclient/build upgrades/webclient
cp ../controlCardImage/board/microchip/sama5d2_som/rootfs-overlay/etc/init.d/S25watchdogd upgrades/

# spawn a separate process for every ip
for ip in $(echo $1.{0..255}); do
	{
		# try to upgrade every 10 seconds
		while sleep 10
		do
			sshpass -p obelisk ssh -o ConnectTimeout=5 root@$ip "$setup_cmd" &>/dev/null
			case "$?" in
			  0) ;;
			 93) echo -e "${GREEN}$ip: already upgraded"; continue ;;
			  6) echo -e "${GRAY}$ip: not an obelisk"; continue ;;
			255) continue ;; # connection refused
			  *) echo -e "$ip: unknown error $?"; continue ;;
			esac

			sshpass -p obelisk scp -r upgrades/* cgminer/cgminer root@$ip:/tmp/upgrades #&>/dev/null
			case "$?" in
			  0) ;;
			  *) echo -e "$ip: unknown error $?"; continue ;;
			esac

			sshpass -p obelisk ssh root@$ip "$finish_cmd" #&>/dev/null
			case "$?" in
			  0) echo -e "${LIGHTGREEN}$ip: upgrade complete!" ;;
			  *) echo -e "$ip: unknown error $?"; continue ;;
			esac
		done
	} &
done

wait # block forever
