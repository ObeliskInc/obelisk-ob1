#!/bin/bash

if [ -z $1 ]; then
	echo "Usage: $0 [subnet]"
	exit 1
fi

LIGHTGREEN='\033[1;32m'
NC='\033[0m'

# spawn a separate process for every ip
for ip in $(echo $1.{0..255}); do
	{
		sshpass -p obelisk ssh -o ConnectTimeout=5 root@$ip 'killall -q burnin; rm -f /root/.upgrade1_complete' &>/dev/null
		case "$?" in
		  0) echo -e "${LIGHTGREEN}$ip: reset successful${NC}" ;;
		  5) ;; # not an obelisk
		  6) ;; # not an obelisk
		127) echo -e "You forgot to install sshpass!"; exit 1 ;;
		255) ;; # connection refused
		  *) echo -e "$ip: unknown error $?"; ;;
		esac
	} &
done

wait

