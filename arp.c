#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>

#define ETHER_TYPE_ARP 0x0806
#define PROTOCOL_TYPE 0x0800
#define MAC_ADDR_LEN 6
#define BROADCAST_MAC "\xff\xff\xff\xff\xff\xff" // 广播地址
#define UNICAST_MAC "\x00\x00\x00\x00\x00\x00"

typedef struct __attribute__((__packed__))
{
    uint8_t dst_mac[MAC_ADDR_LEN];
    uint8_t src_mac[MAC_ADDR_LEN];
    uint16_t ether_type;
} arp_hdr;

typedef struct __attribute__((__packed__))
{
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t hardware_addr_len;
    uint8_t protocol_addr_len;
    uint16_t operation;
    uint8_t src_mac[MAC_ADDR_LEN];
    in_addr_t src_ip;
    uint8_t dst_mac[MAC_ADDR_LEN];
    in_addr_t dst_ip;
} arp_payload;

/*得到网卡ipv4地址*/
void get_card_ipv4(char *network_card_name, in_addr_t *ipv4_addr)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        perror("socket error");
        return;
    }
    struct ifreq ifreq;
    strncpy(ifreq.ifr_name, network_card_name, sizeof(network_card_name) - 1);
    ifreq.ifr_name[sizeof(network_card_name)] = 0;
    if (ioctl(sock, SIOCGIFADDR, &ifreq) < 0)
    {
        perror("get ip name error");
        exit(0);
    }
    struct sockaddr_in *addr = (struct sockaddr_in *)&ifreq.ifr_addr;
    memcpy(ipv4_addr, &addr->sin_addr, sizeof(in_addr_t)); // 替换为你的发送方IP地址
    // for (int i = 0; i < sizeof(in_addr_t); i++)
    // {
    //     printf("%02x ", *((uint8_t *)ipv4_addr + i));
    // }
    // printf("\n");
    close(sock);
}

/*得到网卡MAC地址*/
void get_card_mac(char *network_card_name, uint8_t *mac_addr)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("socket error");
        return;
    }
    struct ifreq ifreq;
    strncpy(ifreq.ifr_name, network_card_name, sizeof(network_card_name) - 1);
    ifreq.ifr_name[sizeof(network_card_name)] = 0;
    if (ioctl(sock, SIOCGIFHWADDR, &ifreq) < 0)
    {
        perror("get mac addr error");
        exit(0);
    }
    memcpy(mac_addr, ifreq.ifr_hwaddr.sa_data, MAC_ADDR_LEN);
    // for (int i = 0; i < MAC_ADDR_LEN; i++)
    // {
    //     printf("%02x ", *((uint8_t *)mac_addr + i));
    // }
    // printf("\n");
    close(sock);
}

/*arp解析ip对应mac*/
void get_target_ip_byarp(char *network_card_name, char *dst_ip, uint8_t *target_mac_addr)
{
    // 创建原始套接字
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0)
    {
        perror("socket");
        exit(1);
    }

    // 设置网络接口
    struct sockaddr_ll sa;
    memset(&sa, 0, sizeof(struct sockaddr_ll));
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_ALL);
    sa.sll_ifindex = if_nametoindex(network_card_name); // 替换为你的网络接口名

    // 绑定套接字到网络接口
    if (bind(sock, (struct sockaddr *)&sa, sizeof(struct sockaddr_ll)) < 0)
    {
        perror("bind");
        close(sock);
        exit(1);
    }

    uint8_t arp_data[sizeof(arp_hdr) + sizeof(arp_payload)];
    memset(arp_data, 0, sizeof(arp_data));

    arp_hdr *arp_hdr_p = (arp_hdr *)arp_data;
    arp_hdr_p->ether_type = htons(ETHER_TYPE_ARP);
    get_card_mac(network_card_name, arp_hdr_p->src_mac);
    memcpy(arp_hdr_p->dst_mac, BROADCAST_MAC, 6);

    arp_payload *arp_req_p = (arp_payload *)(arp_data + sizeof(arp_hdr));
    arp_req_p->hardware_type = htons(ARPHRD_ETHER);
    arp_req_p->protocol_type = htons(PROTOCOL_TYPE);
    arp_req_p->hardware_addr_len = 6;
    arp_req_p->protocol_addr_len = 4;
    arp_req_p->operation = htons(ARPOP_REQUEST);
    memcpy(arp_req_p->src_mac, arp_hdr_p->src_mac, 6); // 替换为你的发送方MAC地址
    memcpy(arp_req_p->dst_mac, UNICAST_MAC, 6);

    get_card_ipv4(network_card_name, &arp_req_p->src_ip);
    in_addr_t dst_ip_addr = inet_addr(dst_ip);
    memcpy(&arp_req_p->dst_ip, &dst_ip_addr, 4); // 替换为你的目标IP地址

    // 发送ARP请求包
    ssize_t sent = sendto(sock, arp_data, sizeof(arp_data), 0, (struct sockaddr *)&sa, sizeof(struct sockaddr_ll));
    if (sent < 0)
    {
        perror("sendto");
        close(sock);
        exit(1);
    }

    // 接收ARP响应包
    while (1)
    {
        unsigned char buffer[65536];
        ssize_t buflen = recv(sock, buffer, sizeof(buffer), 0);

        if (buflen < 0)
        {
            perror("recv");
            break;
        }

        // 检查以太网帧类型是否为ARP
        struct ethhdr *eth = (struct ethhdr *)buffer;
        if (ntohs(eth->h_proto) == ETHER_TYPE_ARP)
        {
            // 解析ARP数据包
            arp_payload *arp_resp = (arp_payload *)(buffer + sizeof(arp_hdr));
            if (ntohs(arp_resp->operation) == ARPOP_REPLY)
            {
                // 检查目标IP地址是否与发送方IP地址以及源地址匹配
                if (memcmp(&arp_resp->src_ip, &arp_req_p->dst_ip, 4) == 0 && memcmp(&arp_resp->dst_ip, &arp_req_p->src_ip, 4) == 0)
                {
                    memcpy(target_mac_addr, &arp_resp->src_mac, MAC_ADDR_LEN);
                    break;
                }
            }
        }
    }
    close(sock);
}
int main(int argc, void **argv)
{
    uint8_t target_mac_addr[MAC_ADDR_LEN];
    get_target_ip_byarp(argv[1], argv[2], target_mac_addr);
    // 输出目标MAC地址
    printf("Received ARP reply from %02x:%02x:%02x:%02x:%02x:%02x\n",
           target_mac_addr[0], target_mac_addr[1],
           target_mac_addr[2], target_mac_addr[3],
           target_mac_addr[4], target_mac_addr[5]);
    return 0;
}
