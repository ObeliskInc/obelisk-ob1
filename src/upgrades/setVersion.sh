#!/bin/bash

ipbase="192.168.10."
ips=( 174 174 )
for ip in "${ips[@]}"
do
	echo "Set version on $ipbase$ip to: $1"
	ssh -t root@$ipbase$ip "echo $1 > /root/.version" || :
	echo "Done!"
done

