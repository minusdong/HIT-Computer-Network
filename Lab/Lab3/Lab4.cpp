/*
* THIS FILE IS FOR IP TEST
*/
// system support
#include "sysInclude.h"
#include <stdio.h>
#include <malloc.h>

extern void ip_DiscardPkt(char *pBuffer, int type);

extern void ip_SendtoLower(char *pBuffer, int length);

extern void ip_SendtoUp(char *pBuffer, int length);

extern unsigned int getIpv4Address();

// implemented by students

int stud_ip_recv(char *pBuffer, unsigned short length) {
    // IP version 0-3bit
    unsigned int ver = pBuffer[0] >> 4;
    // head length 4-7bit
    int head_length = pBuffer[0] & 0xF;
    // ttl 8th Byte
    short ttl = (unsigned short)pBuffer[8];
    // checksum 10th Byte
    short checksum = ntohs(*(unsigned short *)(pBuffer + 10));
    // destination IP address 16-19 byte
    int dest = ntohl(*(unsigned int *)(pBuffer + 16));

    if (ttl <= 0) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_TTL_ERROR);
        return 1;
    }
    if (ver != 4) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_VERSION_ERROR);
        return 1;
    }
    if (head_length < 5) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_HEADLEN_ERROR);
        return 1;
    }
    if (dest != getIpv4Address() && dest != 0xffff) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_DESTINATION_ERROR);
        return 1;
    }

    unsigned short sum = 0;
    unsigned short temp = 0;
    for (int i = 0; i < head_length * 2; i++) {
        temp = 0;
        temp += ((unsigned char)pBuffer[i * 2] << 8);
        temp += (unsigned char)pBuffer[i * 2 + 1];
        if (0xFFFF - sum < temp)
            sum = sum + temp + 1;
        else
            sum = sum + temp;
    }
    if (sum != 0xffff) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_CHECKSUM_ERROR);
        return 1;
    }

    ip_SendtoUp(pBuffer, length);
    return 0;
}

int stud_ip_Upsend(char *pBuffer, unsigned short len, unsigned int srcAddr, 
                    unsigned int dstAddr, byte protocol, byte ttl) {
    // head default 20Bytes
    short ip_length = len + 20;
    char *buffer = (char *)malloc(ip_length * sizeof(char));
    memset(buffer, 0, ip_length);
    // version + head length
    buffer[0] = 0x45;
    buffer[8] = ttl;
    buffer[9] = protocol;
    // packet length
    unsigned short total_length = htons(ip_length);
    memcpy(buffer + 2, &total_length, 2);

    unsigned int src = htonl(srcAddr);
    unsigned int dst = htonl(dstAddr);
    memcpy(buffer + 12, &src, 4);
    memcpy(buffer + 16, &dst, 4);

    unsigned short sum = 0;
    unsigned short temp = 0;

    // checksum
    for (int i = 0; i < 10; i++) {
        temp = (unsigned char)buffer[i  *2] << 8;
        temp += (unsigned char)buffer[i * 2 + 1];
        if (0xffff - sum < temp)
            sum = sum + temp + 1;
        else
            sum = sum + temp;
    }
    unsigned short head_checksum = htons(0xffff - sum);
    memcpy(buffer + 10, &head_checksum, 2);
    memcpy(buffer + 20, pBuffer, len);
    ip_SendtoLower(buffer, len + 20);
    return 0;
}
