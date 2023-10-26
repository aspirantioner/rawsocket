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
#include <sys/time.h>

#define MAC_LEN 6
#define TAG_PARAMS_LEN 1024                                                                                                                                                                             // TAG 数据最大长度
uint8_t radiotap_hdr_arr[] = {0, 0, 30, 0, 0x2e, 0x40, 0x00, 0xa0, 0x20, 0x08, 0x00, 0xa0, 0x20, 0x08, 0x00, 0x00, 0x00, 0x02, 0x9e, 0x09, 0xa0, 0x00, 0xc2, 0x00, 0x00, 0x00, 0xc2, 0x00, 0xc2, 0x01}; // 信标帧常用数据

typedef struct __attribute__((__packed__))
{
    uint32_t present_flag_words[3];
} present_flags;

typedef struct __attribute__((__packed__))
{
    uint8_t revision;
    uint8_t pad;
    uint16_t hdr_len;
    present_flags present_words;
    uint8_t flags;
    uint8_t data_rate;
    uint16_t channel_freq;
    uint16_t channel_flags;
    int8_t antenna_sig;
    uint8_t padd;
    uint16_t rx_flag;
    int8_t antenna_sig1;
    uint8_t antenna_value1;
    int8_t antenna_sig2;
    uint8_t antenna_value2;
} radiotap_hdr;

typedef struct __attribute__((__packed__))
{
    uint8_t version : 2;
    uint8_t type : 2;
    uint8_t sub_type : 4;
    uint8_t flags;
} control_field;

typedef struct __attribute__((__packed__))
{
    control_field field;
    uint16_t duration;
    uint8_t dst_mac[MAC_LEN];
    uint8_t src_mac[MAC_LEN];
    uint8_t bss_id[MAC_LEN];
    uint16_t fragment_number : 4;
    uint16_t seq_number : 12;
} beacon_frame;

typedef struct __attribute__((__packed__))
{
    uint64_t time_stamp;
    uint16_t beacon_interval;
    uint16_t capabilities;
} fixed_params;

typedef struct __attribute__((__packed__))
{
    fixed_params fixed_hdr;
    uint8_t tag_params[TAG_PARAMS_LEN];
    uint32_t tag_params_offset;
} manage_pkt;

typedef struct __attribute__((__packed__))
{
    uint8_t dtim_count;
    uint8_t dtim_period;
    uint8_t multicast : 1;
    uint8_t bitmap_offset : 7;
    uint8_t partial_virtual_bitmap;
} tim;

typedef struct __attribute__((__packed__))
{
    char code[2];
    uint8_t environment;
    uint8_t first_channel_num;
    uint8_t channel_num;
    uint8_t max_trans_power_level;
} country_info;

typedef struct __attribute__((__packed__))
{
    uint8_t non_erp_present : 1;
    uint8_t use_protection : 1;
    uint8_t barker_preamble_mode : 1;
    uint8_t reserved : 5;
} erp_info;

typedef struct __attribute__((__packed__))
{
    uint8_t rx_modulation[10];
    uint16_t highest_support_data_rate;
    uint8_t tx_support_mcs_set : 1;
    uint8_t tx_rx_mcs_set : 1;
    uint8_t maxnum_of_tx : 2;
    uint8_t unequal_modulation : 1;
    uint8_t padd[3];
} rx_support;

typedef struct __attribute__((__packed__))
{
    uint16_t ht_capabilities;
    uint8_t a_mpdu_param;
    rx_support rx_sup;
    uint16_t ht_capabilities_extend;
    uint32_t tx_beam_form_capa;
    uint8_t asel;
} ht_capabilities;

typedef struct __attribute__((__packed__))
{
    uint8_t rsn_version;
    uint8_t group_cipher_suite_qui[3];
    uint8_t group_cipher_suite_type;

    uint8_t pairwise_cipher_suite_count;
    uint8_t pairwise_cipher_suite_qui[3];
    uint8_t pairwise_cipher_suite_type;

    uint8_t auth_key_management_suite_count;
    uint8_t auth_key_management_qui[3];
    uint8_t auth_key_management_type;

    uint16_t rsn_capabilities;
} rsn_info;

typedef struct __attribute__((__packed__))
{
    uint8_t primary_channel;
    uint8_t ht_info_subset1;
    uint16_t ht_info_subset2;
    uint16_t ht_info_subset3;
    rx_support rx_sup;
} ht_info;

void manage_pkt_init(manage_pkt *manage_pkt_p, uint64_t time_stamp, uint16_t beacon_interval, uint32_t capabilities)
{
    manage_pkt_p->tag_params_offset = 0;
    manage_pkt_p->fixed_hdr.time_stamp = time_stamp;
    manage_pkt_p->fixed_hdr.beacon_interval = beacon_interval;
    manage_pkt_p->fixed_hdr.capabilities = capabilities;
}

void manage_pkt_fill(manage_pkt *manage_pkt_p, uint8_t tag_type, void *tag_data, uint8_t tag_data_len)
{
    uint32_t offset = manage_pkt_p->tag_params_offset;
    manage_pkt_p->tag_params[offset++] = tag_type;
    manage_pkt_p->tag_params[offset++] = tag_data_len;

    memcpy(manage_pkt_p->tag_params + offset, tag_data, tag_data_len);
    offset += tag_data_len;
    manage_pkt_p->tag_params_offset = offset;
}

int main(void)
{
    const char *network_card_name = "wlan0mon"; // 网卡名

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

    // 设置promisc模式
    struct packet_mreq t_mr;
    memset(&t_mr, 0, sizeof(t_mr));
    t_mr.mr_ifindex = sa.sll_ifindex;
    t_mr.mr_type = PACKET_MR_PROMISC;
    if (setsockopt(sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &t_mr, sizeof(t_mr)) < 0)
    {
        perror("setsocket promisc error");
        close(sock);
        exit(1);
    }

    uint8_t send_data[sizeof(radiotap_hdr) + sizeof(beacon_frame) + sizeof(manage_pkt)];

    // 设置信标帧
    radiotap_hdr *radio_hdr_p = (radiotap_hdr *)send_data;
    memcpy(radio_hdr_p, radiotap_hdr_arr, sizeof(radiotap_hdr_arr));

    // 设置beacon帧
    beacon_frame *beacon_pkt_p = (beacon_frame *)(send_data + sizeof(radiotap_hdr));
    beacon_pkt_p->field.version = 0;
    beacon_pkt_p->field.type = 0;
    beacon_pkt_p->field.sub_type = 8;
    beacon_pkt_p->field.flags = 0;
    beacon_pkt_p->duration = 0;
    memcpy(beacon_pkt_p->dst_mac, "\xff\xff\xff\xff\xff\xff", 6);
    memcpy(beacon_pkt_p->src_mac, "\x01\x02\x03\x04\x05\x06", 6);
    memcpy(beacon_pkt_p->bss_id, "\x01\x02\x03\x04\x05\x06", 6);
    beacon_pkt_p->fragment_number = 0;
    beacon_pkt_p->seq_number = 789;

    // 设置管理帧
    manage_pkt *manage_pkt_p = (manage_pkt *)(send_data + sizeof(radiotap_hdr) + sizeof(beacon_frame));
    // 管理帧固定前12字节
    manage_pkt_p->fixed_hdr.time_stamp = 106714931194;
    manage_pkt_p->fixed_hdr.beacon_interval = 0x64;
    manage_pkt_p->fixed_hdr.capabilities = 0x1421;

    // 管理帧ssid
    manage_pkt_fill(manage_pkt_p, 0, "ljftest2", sizeof("ljftest2") - 1);

    // 管理帧支持速度
    uint8_t sup_rates_arr[] = {0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24};
    manage_pkt_fill(manage_pkt_p, 1, sup_rates_arr, sizeof(sup_rates_arr));

    // 管理帧现在信道
    uint8_t cur_channel = 11;
    manage_pkt_fill(manage_pkt_p, 3, &cur_channel, 1);

    // 管理帧dtim
    tim tim_pkt;
    tim_pkt.dtim_count = 1;
    tim_pkt.dtim_period = 2;
    tim_pkt.bitmap_offset = 0;
    tim_pkt.multicast = 0;
    tim_pkt.partial_virtual_bitmap = 0;
    manage_pkt_fill(manage_pkt_p, 5, &tim_pkt, sizeof(tim));

    // 管理帧区域信息
    uint8_t country_arr[] = {'C', 'N', 4, 1, 13, 23, 36, 8, 23, 149, 5, 23};
    manage_pkt_fill(manage_pkt_p, 7, country_arr, sizeof(country_arr));

    erp_info erp_info_pkt;
    *(uint8_t *)&erp_info_pkt = 0;
    manage_pkt_fill(manage_pkt_p, 42, &erp_info_pkt, sizeof(erp_info));

    uint8_t extend_sup_rates_arr[] = {12, 18, 24, 36, 48, 72, 96, 108};
    manage_pkt_fill(manage_pkt_p, 50, extend_sup_rates_arr, sizeof(extend_sup_rates_arr));

    ht_capabilities ht_capa_pkt;
    ht_capa_pkt.asel = 0;
    ht_capa_pkt.ht_capabilities_extend = 0;
    ht_capa_pkt.ht_capabilities = 0x00ef;
    ht_capa_pkt.tx_beam_form_capa = 0x01000000;
    ht_capa_pkt.a_mpdu_param = 0x13;
    uint64_t arr[2] = {0x000000000000ffff, 0};
    memcpy(&ht_capa_pkt.rx_sup, (void *)arr, 16);
    manage_pkt_fill(manage_pkt_p, 45, &ht_capa_pkt, sizeof(ht_capabilities));

    ht_info ht_info_pkt;
    arr[0] = 0;
    memcpy(&ht_info_pkt.rx_sup, (void *)arr, 16);
    ht_info_pkt.ht_info_subset1 = 0;
    ht_info_pkt.ht_info_subset2 = 0;
    ht_info_pkt.ht_info_subset3 = 0;
    ht_info_pkt.primary_channel = 11;
    manage_pkt_fill(manage_pkt_p, 61, &ht_info_pkt, sizeof(ht_info));

    manage_pkt_fill(manage_pkt_p, 221, "\x00\x50\xf2\x02\x01\x01\x81\x00\x02\x32\x00\x00\x22\x32\x00\x00\x42\x32\x5e\x00\x62\x32\x2f\x00", 24);
    manage_pkt_fill(manage_pkt_p, 191, "\x92\x70\x80\x33\xfa\xff\x62\x03\xfa\xff\x62\x03", 12);
    manage_pkt_fill(manage_pkt_p, 192, "\x00\x00\x00\xfa\xff", 5);
    manage_pkt_fill(manage_pkt_p, 195, "\x00\x2e", 2);
    manage_pkt_fill(manage_pkt_p, 127, "\x05\x00\x00\x00\x00\x00\x00\x40", 8);
    manage_pkt_fill(manage_pkt_p, 221, "\x8c\xfd\xf0\x01\x01\x02\x01\x00\x02\x01\x01", 11);
    uint32_t send_size = sizeof(radiotap_hdr) + sizeof(beacon_frame) + sizeof(fixed_params) + manage_pkt_p->tag_params_offset;

    for (int i = 0;; i++)
    {
        struct timeval t_time;
        gettimeofday(&t_time, 0);
        manage_pkt_p->fixed_hdr.time_stamp = ((uint64_t)t_time.tv_sec) * 1000000 + t_time.tv_usec;
        beacon_pkt_p->seq_number++;
        if (write(sock, send_data, send_size) != send_size)
        {
            perror("send failed!\n");
        };
        usleep(100000);
    }

    return 0;
}