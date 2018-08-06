//===========================================================================
// Copyright 2018 Obelisk Inc.
//===========================================================================

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#define MDNS_PORT 5353
#define MDNS_GROUP "224.0.0.251"

int send_udp(const char* group, int port, const char* packet, int mlen)
{
    // create UDP socket
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return fd;
    }

    // set up destination address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(group);
    addr.sin_port = htons(port);

    // send packet
    return sendto(fd, packet, mlen, 0, (struct sockaddr*)&addr, sizeof(addr));
}

int send_mDNS_response(const char* name, const char* ip_v4_address, uint32_t ttl)
{
    // construct domain and record fields
    char domain[256];
    memset(domain, 0, sizeof(domain));
    domain[0] = strlen(name);
    strcpy(domain + 1, name);
    domain[1 + strlen(name)] = strlen("local");
    strcat(domain, "local");

    uint16_t type = htons(1); // 'A' record
    uint16_t class = htons(1); // Internet
    ttl = htonl(ttl);
    uint32_t rdata = inet_addr(ip_v4_address);
    uint16_t rdlen = htons(sizeof(rdata));

    // construct DNS message
    char buf[1024];
    memset(&buf, 0, sizeof(buf));
    int n = 0;

    buf[n + 0] = 0xAA;
    buf[n + 1] = 0xAA; // id (TODO: what should this be?)
    buf[n + 2] = 0x84; // flags: qr = 1, aa = 1
    buf[n + 7] = 0x01; // answer count
    n = 12;
    memcpy(&buf[n], domain, strlen(domain) + 1);
    n += strlen(domain) + 1;
    memcpy(&buf[n], (char*)&type, sizeof(type));
    n += sizeof(type);
    memcpy(&buf[n], (char*)&class, sizeof(class));
    n += sizeof(class);
    memcpy(&buf[n], (char*)&ttl, sizeof(ttl));
    n += sizeof(ttl);
    memcpy(&buf[n], (char*)&rdlen, sizeof(rdlen));
    n += sizeof(rdlen);
    memcpy(&buf[n], (char*)&rdata, sizeof(rdata));
    n += sizeof(rdata);

    return send_udp(MDNS_GROUP, MDNS_PORT, buf, n);
}
