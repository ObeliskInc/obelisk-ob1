#!/bin/bash

sshpass -p obelisk scp example.txt root@192.168.86.197:/tmp/example.txt

# Log into the machine and dd from /tmp to /root/example
sshpass -p obelisk ssh root@192.168.86.197 << !
	ls
	touch blankFile
	dd if=/dev/zero of=blankFile bs=100 count=1
	dd if=/tmp/example.txt of=/root/example.txt
	exit
!
