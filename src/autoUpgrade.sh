#!/bin/bash

if [ -z $1 ]; then
	echo "Usage: $0 [subnet]"
	exit 1
fi

GRAY='\033[0;37m'
GREEN='\033[0;32m'
LIGHTGREEN='\033[1;32m'
NC='\033[0m'

setup_cmd=`cat <<EOF
if [ -e /root/.upgrade1_complete ]
then
	exit 93
fi

rm -rf /tmp/upgrades
mkdir -p /tmp/upgrades

/etc/init.d/S25watchdogd stop &>/dev/null
killall -q cgminer
killall -q apiserver
exit 0
EOF
`

finish_cmd='/tmp/upgrades/detect; if [ "$?" != "3" ]' # prevent bash from expanding $? prematurely
finish_cmd+=`cat <<EOF

then
	nohup /usr/sbin/led_flash_red &>/dev/null &
	exit 77
fi
mv /tmp/upgrades/led_flash_green /usr/sbin/led_flash_green
mv /tmp/upgrades/led_flash_red /usr/sbin/led_flash_red
mv /tmp/upgrades/led_alternate /usr/sbin/led_alternate
mv /tmp/upgrades/led_off /usr/sbin/led_off
mv /tmp/upgrades/cgminer /usr/sbin/cgminer
mkdir -p /root/.cgminer
cp /tmp/upgrades/cgminer.conf /root/.cgminer/cgminer.conf
mv /tmp/upgrades/cgminer.conf /root/.cgminer/default_cgminer.conf
mv /tmp/upgrades/apiserver /usr/sbin/apiserver
rm -rf /var/www/*
mv /tmp/upgrades/webclient/* /var/www/
mv /tmp/upgrades/S25watchdogd /etc/init.d/S25watchdogd
mv /tmp/upgrades/watchdog.sh /usr/sbin/watchdog.sh
mv /tmp/upgrades/burn-in /tmp/burn-in
rm -rf /tmp/upgrades

killall -q led_alternate
killall -q led_flash_red
killall -q led_flash_green
/usr/sbin/led_alternate &

touch /root/.upgrade1_complete

if /tmp/burn-in -T &>/dev/null; then
	killall -q led_alternate
	killall -q led_flash_red
	killall -q led_flash_green
	nohup /usr/sbin/led_flash_green &>/dev/null &
else
	killall -q led_alternate
	killall -q led_flash_red
	killall -q led_flash_green
	nohup /usr/sbin/led_flash_red &>/dev/null &
	exit 50
fi

exit 0
EOF
`

# create upgrades dir
rm -rf upgrades
mkdir upgrades

cp controlCardUtils/bin/led_flash_green upgrades/
cp controlCardUtils/bin/led_flash_red upgrades/
cp controlCardUtils/bin/led_alternate upgrades/
cp controlCardUtils/bin/led_off upgrades/
cp cgminer/cgminer upgrades/
cp ../controlCardImage/board/microchip/sama5d2_som/rootfs-overlay/root/.cgminer/cgminer.conf upgrades/
cp apiserver/bin/apiserver upgrades/
cp -R webclient/build upgrades/webclient
cp ../controlCardImage/board/microchip/sama5d2_som/rootfs-overlay/etc/init.d/S25watchdogd upgrades/
cp ../controlCardImage/board/microchip/sama5d2_som/rootfs-overlay/usr/sbin/watchdog.sh upgrades/
cp detect upgrades/
cp burn-in upgrades/

# spawn a separate process for every ip
for ip in $(echo $1.{0..255}); do
	{
		# try to upgrade every 10 seconds
		while sleep 10
		do
			sshpass -p obelisk ssh -o ConnectTimeout=5 root@$ip "$setup_cmd" &>/dev/null
			case "$?" in
			  0) ;;
			 93) echo -e "${GREEN}$ip: already upgraded${NC}"; continue ;;
			  5) echo -e "${GRAY}$ip: not an obelisk${NC}"; continue ;;
			  6) echo -e "${GRAY}$ip: not an obelisk${NC}"; continue ;;
			127) echo -e "You forgot to install sshpass!"; exit 1 ;;
			255) continue ;; # connection refused
			  *) echo -e "$ip: unknown error $?"; continue ;;
			esac

			sshpass -p obelisk scp -r upgrades/* root@$ip:/tmp/upgrades #&>/dev/null
			case "$?" in
			  0) ;;
			  *) echo -e "$ip: unknown error $?"; continue ;;
			esac

			sshpass -p obelisk ssh root@$ip "$finish_cmd" &>/dev/null
			case "$?" in
			  0) echo -e "${LIGHTGREEN}$ip: upgrade complete!${NC}" ;;
			 50) echo -e "$ip: burn-in test failed"; continue ;;
			 77) echo -e "${GRAY}$ip: not an SC1"; continue ;;
			  *) echo -e "$ip: unknown error $?"; continue ;;
			esac
		done
	} &
done

wait # block forever
