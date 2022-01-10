/*
* THIS FILE IS FOR IP FORWARD TEST
*/
#include "sysInclude.h"
#include <vector>
#include <iostream>

using std::vector;
using std::cout;

// system support
extern void fwd_LocalRcv(char *pBuffer, int length);

extern void fwd_SendtoLower(char *pBuffer, int length, unsigned int nexthop);

extern void fwd_DiscardPkt(char *pBuffer, int type);

extern unsigned int getIpv4Address();

// implemented by students

struct routeTableItem {
    unsigned int destIP;     // 目的IP
    unsigned int mask;       // 掩码
    unsigned int masklen;    // 掩码长度
    unsigned int nexthop;    // 下一跳
};

vector<routeTableItem> m_table; 

void stud_Route_Init() {
    m_table.clear();
}

void stud_route_add(stud_route_msg *proute) {
    routeTableItem newTableItem;
    newTableItem.masklen = ntohl(proute->masklen);
    newTableItem.mask = (1 << 31) >> (ntohl(proute->masklen) - 1);
    newTableItem.destIP = ntohl(proute->dest);
    newTableItem.nexthop = ntohl(proute->nexthop);
    m_table.push_back(newTableItem);
}

int stud_fwd_deal(char *pBuffer, int length) {

    int TTL = (int)pBuffer[8];
    int headerChecksum = ntohl(*(unsigned short*)(pBuffer + 10)); 
    int DestIP = ntohl(*(unsigned int*)(pBuffer+16));
    int headsum = pBuffer[0] & 0xF; 

    // 判断分组地址与本机地址是否相同
    if(DestIP == getIpv4Address()) {
        fwd_LocalRcv(pBuffer, length);
        return 0;
    }
    // TTL < 0, cannot forward
    if(TTL <= 0) {
        // Drop packet
        fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR);
        return 1;
    }

    //设置匹配位
    bool Match = false;
    unsigned int longestMatchLen = 0;
    int bestMatch = 0;
    for(int i = 0; i < m_table.size(); i ++) {
        if(m_table[i].masklen > longestMatchLen && m_table[i].destIP == (DestIP & m_table[i].mask)) {
            bestMatch = i;
            Match = true;
            longestMatchLen = m_table[i].masklen;
        }
    }

    if(Match) {
        char *buffer = new char[length];
        memcpy(buffer,pBuffer,length);
        // TTL - 1
        buffer[8]--;
        int sum = 0;
        unsigned short int localCheckSum = 0;
        for(int j = 1; j < 2 * headsum +1; j ++) {
            if (j != 6) { 
                sum = sum + (buffer[(j-1)*2]<<8)+(buffer[(j-1)*2+1]);
                sum %= 65535; 
            }
        }
        // checksum
        localCheckSum = htons(~(unsigned short int)sum);
        memcpy(buffer+10, &localCheckSum, sizeof(unsigned short));
        // Send to Lower protocol
        fwd_SendtoLower(buffer, length, m_table[bestMatch].nexthop);
        return 0;
    }
    else {
        fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE);
        return 1;
    }
    return 1;
}