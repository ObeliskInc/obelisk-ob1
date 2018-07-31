#!/bin/sh

# Declare commands to check for.
cmd1="sleep 30s &"
cmd2="sleep 30s &"

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

	# Sleep for a second before trying again
	sleep 1s
done
