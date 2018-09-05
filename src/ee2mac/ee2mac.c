/*
 * ee2mac - Retrieve a MAC address from I2C EEPROM or 
 * EUI48/EUI64 devices (AT24MACxxx or 24AAxxE48/24AAxxE64) and update
 * target network adapter configuration.
 * 
 * Copyright 2018 Microchip
 * 		  Matt Wood <matt.wood@microchip.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <assert.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <net/if.h>

void help(void)
{
	printf("Usage: ee2mac [-i IFACE] [-e EEPROM] [-o OFFSET]\r\n" \
			"\t-i\tInferface to use (default eth0)\r\n" \
			"\t-eEEPROM\tI2C EEPROM bus node (i.e /sys/bus/i2c/devices/3-0050/eeprom)\r\n" \
			"\t-o\t\tOffset of MAC address within EEPROM memory (i.e 0xfa)\r\n");
}

int main(int argc, char **argv) {
	struct ifreq iface;
	char *iface_name = "eth0";
	char *eeprom = NULL;
	char iface_status_loc[50] = "/sys/class/net/";
	char iface_status[5] = {0,};
	int offset;
	int sock;
	int opt_index = 0;
	char *strptr;
	FILE *fp;
	char mac_addr[9];	/* TODO: add IPV6 support */
	char bytes_read = 0;
	int ret;
	int i;
	
	while ((opt_index = getopt(argc, argv, "e:o:i:")) != -1) {
		
		switch (opt_index) {				
			case 'i':
				if (optarg) {
					iface_name = optarg;
				}			
				break;
				
			case 'e':
				eeprom = optarg;
				break;
				
			case 'o':
				if (strptr = strpbrk(optarg, "xX")) {
					offset = strtol(optarg, NULL, 0);
				} else if (strptr = strpbrk(optarg, "aAbBcCdDeEfF")) {
					offset = strtol(optarg, NULL, 16);
				} else {
					offset = strtol(optarg, NULL, 10);
				}
				break;
				
			default:
				help();
				exit(0);
				break;
		};
	}

	// handle empty arguments
	if ((opt_index == -1) && (argc == 1)) {
		help();
		exit(0);
	}
	
	strcat(iface_status_loc, iface_name);
	strcat(iface_status_loc, "/operstate");

	fp = fopen(iface_status_loc, "r");
	if (fp == NULL) {
		printf("Error: Invalid network adapter, %s\r\n", iface_name);
		exit(0);
	}
	
	ret = fread(iface_status, 1, 4, fp);
	
	if (ret) {
		if (strcmp(iface_status, "down")) {
			printf("Error: Network interface %s is up, cannot set MAC address until it is disabled\r\n", iface_name);
			exit(0);
		}
	} else {
		printf("Error: Cannot determine interface operating status\r\n");
		exit(0);
	}
	
	fclose(fp);
	
	fp = fopen(eeprom, "r");
	if (fp != NULL) {
		ret = fseek(fp, offset, SEEK_SET);
		if (ret) {
			printf("Cannot set offset of EEPROM\r\n");
			help();
			return -EIO;
		}
			
		bytes_read = fread(mac_addr, 1, 6, fp);
		fclose(fp);
	
		if (bytes_read) {			
			printf("MAC Address will be set to: %02X:%02X:%02X:%02X:%02X:%02X\r\n", \
					mac_addr[0], mac_addr[1], mac_addr[2], \
					mac_addr[3], mac_addr[4], mac_addr[5]);
		} 
	}
	
	strcpy(iface.ifr_name, iface_name);
	
	for (i=0; i<6; i++) {
		iface.ifr_hwaddr.sa_data[i] = mac_addr[i];
	}
	iface.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	
	if (sock == -1) {
		printf("Error opening socket!\r\n");
		return -EIO;
	}
	
	assert(ioctl(sock, SIOCSIFHWADDR, &iface) != -1);

	return EXIT_SUCCESS;
}
