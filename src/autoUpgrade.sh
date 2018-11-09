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
killall -q burnin cgminer apiserver
exit 0
EOF
`

finish_cmd='/tmp/upgrades/detect; case "$?" in' # prevent bash from expanding $? prematurely
finish_cmd+=`cat <<EOF

  3) mv /tmp/upgrades/cgminer-sia /usr/sbin/cgminer; mv /tmp/upgrades/burnin-sia /tmp/burnin; mv /tmp/upgrades/cgminer-sia.conf /tmp/upgrades/cgminer.conf ;;
  4) mv /tmp/upgrades/cgminer-dcr /usr/sbin/cgminer; mv /tmp/upgrades/burnin-dcr /tmp/burnin; mv /tmp/upgrades/cgminer-dcr.conf /tmp/upgrades/cgminer.conf ;;
  *) (nohup /usr/sbin/led_flash_red &>/dev/null &); exit 77 ;;
esac

mv /tmp/upgrades/led_flash_green /usr/sbin/led_flash_green
mv /tmp/upgrades/led_flash_red /usr/sbin/led_flash_red
mv /tmp/upgrades/led_alternate /usr/sbin/led_alternate
mv /tmp/upgrades/led_off /usr/sbin/led_off
rm -f /root/.cgminer/settings*
mkdir -p /root/.cgminer
cp /tmp/upgrades/cgminer.conf /root/.cgminer/cgminer.conf
mv /tmp/upgrades/cgminer.conf /root/.cgminer/default_cgminer.conf
mv /tmp/upgrades/apiserver /usr/sbin/apiserver
rm -rf /var/www/*
mv /tmp/upgrades/webclient/* /var/www/
chown -R www-data:www-data /var/www/*
mv /tmp/upgrades/S25watchdogd /etc/init.d/S25watchdogd
mv /tmp/upgrades/watchdog.sh /usr/sbin/watchdog.sh
mv /tmp/upgrades/S99upgraderd /etc/init.d/S99upgraderd
mv /tmp/upgrades/upgraderd.sh /usr/sbin/upgraderd.sh
rm -rf /tmp/upgrades

killall -q led_alternate led_flash_red led_flash_green
/usr/sbin/led_alternate &

echo v1.0.0 > /root/.version
touch /root/.upgrade1_complete;

EOF
`
finish_cmd+='STARTTIME=$(date +%s); while [ $(( $(date +%s) - $STARTTIME )) -lt 600 ];'
finish_cmd+=`cat <<EOF

do
	if /tmp/burnin -T &>/dev/null
	then
		killall -q led_alternate led_flash_red led_flash_green
		nohup /usr/sbin/led_flash_green &>/dev/null &
		exit 0
	fi
	echo "Burn-in failed, trying again..." >> /tmp/hashrates
	grep Hashrate /var/log/messages | grep HB0 | tail -n1 >> /tmp/hashrates
	grep Hashrate /var/log/messages | grep HB1 | tail -n1 >> /tmp/hashrates
	grep Hashrate /var/log/messages | grep HB2 | tail -n1 >> /tmp/hashrates
done

killall -q led_alternate led_flash_red led_flash_green
nohup /usr/sbin/led_flash_red &>/dev/null &

exit 50

EOF
`

echo "Starting upgrade script ($(date +'%b %d %r'))"

# spawn a separate process for every ip
for ip in $(echo $1.{0..255}); do
	{
		# try to upgrade repeatedly forever
		while true
		do
			sshpass -p obelisk ssh -o ConnectTimeout=5 root@$ip "$setup_cmd" &>/dev/null
			case "$?" in
			  0) ;;
			 93) echo -e "${GREEN}$ip: already upgraded (trying again in 5 mins)${NC}"; sleep 5m; continue ;;
			  5) echo -e "${GRAY}$ip: not an obelisk (trying again in 10 mins)${NC}"; sleep 10m; continue ;;
			  6) echo -e "${GRAY}$ip: not an obelisk (trying again in 10 mins)${NC}"; sleep 10m; continue ;;
			127) echo -e "You forgot to install sshpass!"; exit 1 ;;
			255) sleep 1m; continue ;; # connection refused
			  *) echo -e "$ip: unknown error $?"; sleep 2m; continue ;;
			esac

			sshpass -p obelisk scp -r upgrades/* root@$ip:/tmp/upgrades &>/dev/null
			case "$?" in
			  0) ;;
			  *) echo -e "$ip: unknown error $?"; sleep 5m; continue ;;
			esac

			echo "$ip: Starting burn-in test ($(date +'%b %d %r'))"

			sshpass -p obelisk ssh root@$ip "$finish_cmd" &>/dev/null
			case "$?" in
			  0) echo -e "${LIGHTGREEN}$ip: burn-in test passed! ($(date +'%b %d %r'))${NC}"; continue ;;
			 50) echo -e "$ip: burn-in test failed ($(date +'%b %d %r'))"; ;;
			 77) echo -e "${GRAY}$ip: not an SC1 (trying again in 10 mins)"; sleep 10m; continue ;;
			  *) echo -e "$ip: unknown error $?"; sleep 5m; continue ;;
			esac

			# copy hashrate file over
			sshpass -p obelisk scp root@$ip:/tmp/hashrates hashrates-${ip}.log &>/dev/null
			echo "$ip: wrote bad hashrates to hashrates-${ip}.log"
			sleep 5m
		done
	} &
done

wait # block forever
