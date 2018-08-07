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

# watchdog related configs
cmd1="/usr/sbin/apiserver &" # binary 1 to run and watch
cmd2="/usr/sbin/cgminer &" # binary 2 to run and watch

################################################################################
# Run Watchdog
################################################################################

# chown the webclient
chown -R www-data:www-data $WEB_ROOT*

# Run first command and save pid.
eval $cmd1
pid1=$!

# Run second command and save pid.
eval $cmd2
pid2=$!

# Notify users about commands running.
echo "started processes as $pid1 and $pid2"

# Check if the processes run every second.
while true; do
	# Check first process
	if ! kill -0 $pid1 &> /dev/null; 
	then
		echo "Restarting $pid1"
		eval $cmd1
		pid1=$!
		echo "New pid $pid1"
		echo ""
	fi
	
	# Check second process
	if ! kill -0 $pid2 &> /dev/null; 
	then
		echo "Restarting $pid2"
		eval $cmd2
		pid2=$!
		echo "New pid $pid2"
		echo ""
	fi

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
