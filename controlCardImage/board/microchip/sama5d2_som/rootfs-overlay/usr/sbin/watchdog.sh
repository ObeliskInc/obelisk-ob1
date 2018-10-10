#!/bin/sh

################################################################################
# Config
################################################################################

# webclient related configs
WEB_ROOT=/var/www/

# log related configs
LOG_FOLDER=/var/log/          # Folder with logs. Will be searched recursively
SAVELOG_BIN=/usr/sbin/savelog # Path to savelog binary
MAX_LOG_SIZE=40960            # 40 kib
NUM_LOGS=5                    # Max number of copies

# apiserver related configs
APISERVER=apiserver
APISERVER_DAEMON=/usr/sbin/$APISERVER
APISERVER_PIDFILE=/var/run/$APISERVER.pid
APISERVER_ARGS=""

# cgminer related configs
CGMINER=cgminer
CGMINER_DAEMON=/usr/sbin/$CGMINER
CGMINER_PIDFILE=/var/run/$CGMINER.pid
CGMINER_ARGS="--default-config /root/.cgminer/cgminer.conf --api-listen --api-allow W:127.0.0.1 --log 4 --protocol-dump --syslog"

NET_CHECK_CNT=0


################################################################################
# Run Watchdog
################################################################################

# chown the webclient
chown -R www-data:www-data $WEB_ROOT*

# Check if the processes run every second.
while true; do
	# Start/Restart apiserver
	start-stop-daemon --start --chuid root --pidfile $APISERVER_PIDFILE --make-pidfile --background --exec $APISERVER_DAEMON -- $APISERVER_ARGS

	# Start/Restart cgminer
	start-stop-daemon --start --chuid root --pidfile $CGMINER_PIDFILE --make-pidfile --background --exec $CGMINER_DAEMON -- $CGMINER_ARGS

	# Archive large logs in /var/log
	find $LOG_FOLDER -type f ! -regex '.*\.[0-9].*' | while read -r log ; do
		# Get size of log.
		size=`wc -c $log | cut -d' ' -f1`
		case $log in
			"/var/log/upgrade"*) continue
		esac
		# If size greater than 40kib call savelog.
		if [[ $log != */var/log/upgrade/* ]] && [ $size -ge $MAX_LOG_SIZE ] ; then
			eval "$SAVELOG_BIN -c $NUM_LOGS -n $log"
		fi
	done

	# If there is no network connection when we first boot, udhcpc quits.  This check ensures
	# that it gets restarted so that connecting Ethernet later on will still pull an IP
	# address over DHCP.
	RESTART=0
	MODE=cat /root/config/interfaces | sed -En 's/iface(.*) eth0 inet (.*)/\2/p'
	
	# Only start udhcpc if the interface is set to dhcp, not static
	if [ $MODE -eq "dhcp"]
	then
		if [ -f /var/run/udhcpc.eth0.pid ]; then
			UDHCPC_PID=`cat /var/run/udhcpc.eth0.pid`
			if ! kill -0 $UDHCPC_PID
			then
				RESTART=1
			fi
		else
			RESTART=1
		fi
		if [ $RESTART -eq "1" ]
		then
			# udhcpc is not running, so bring eth0 down then up to restart it
			ifdown eth0
			ifup eth0
		fi
	fi

	# Check that the default gateway is still accessible, otherwise it probably means that
	# our network connection is broken, so reboot to try to recover.
	NET_CHECK_CNT=$((NET_CHECK_CNT+1))
	if [ $NET_CHECK_CNT -ge 60 ]
	then
		DEFAULT_GATEWAY=`route -n | awk '/^0.0.0.0/ {print $2}'`
		if [ -z $DEFAULT_GATEWAY ]
		then
			# No gateway means no network
			echo "Unable to determine IP address of default gateway...Rebooting!"
			reboot
			exit 1
		fi

		echo "Pinging $DEFAULT_GATEWAY"
		PING_CNT=`ping -c 1 $DEFAULT_GATEWAY  | awk '/transmitted/ {print $4}'`
		echo "C=$PING_CNT"
		if [ $PING_CNT -ne 1 ]; then
			echo "Unable to ping default gateway...Rebooting!"
			reboot
			exit 1
		else
			echo "SUCCESSFUL REPLY: PING_CNT=$PING_CNT"
		fi

		NET_CHECK_CNT=0
	fi

	# Sleep for a second before trying again
	sleep 1s
done
