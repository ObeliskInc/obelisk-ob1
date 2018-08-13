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
		# If size greater than 40kib call savelog.
		if [ $size -ge $MAX_LOG_SIZE ] ; then
			eval "$SAVELOG_BIN -c $NUM_LOGS -n $log"
		fi
	done

	# Sleep for a second before trying again
	sleep 1s
done
