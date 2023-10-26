#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define MAGIC "1234567890" // ICMP 附带数据
#define MAGIC_LEN 11
#define IP_BUFFER_SIZE 65536
#define RECV_TIMEOUT_USEC 100000

struct __attribute__((__packed__)) icmp_echo
{
    // header
    uint8_t type;
    uint8_t code;
    uint16_t checksum;

    uint16_t ident;
    uint16_t seq;

    // data
    double sending_ts;
    char magic[MAGIC_LEN];
};

double get_timestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + ((double)tv.tv_usec) / 1000000;
}

uint16_t calculate_checksum(unsigned char *buffer, int bytes)
{
    uint32_t checksum = 0;
    unsigned char *end = buffer + bytes;

    // 奇字节补0
    if (bytes % 2 == 1)
    {
        end = buffer + bytes - 1;
        checksum += (*end) << 8;
    }

    // 两字节一组
    while (buffer < end)
    {
        checksum += (buffer[0] << 8) + buffer[1];

        // 高两位不为持续相加
        while (1)
        {
            uint32_t carray = checksum >> 16;
            if (carray != 0)
            {
                checksum = (checksum & 0xffff) + carray;
            }
            else
            {
                break;
            }
        }

        buffer += 2;
    }

    // 取反
    checksum = ~checksum;

    return checksum & 0xffff;
}

int send_echo_request(int sock, struct sockaddr_in *addr, int ident, int seq)
{
    struct icmp_echo icmp;
    bzero(&icmp, sizeof(icmp));

    // 填充相关类型号
    icmp.type = 8;
    icmp.code = 0;
    icmp.ident = htons(ident);
    icmp.seq = htons(seq);

    // 填充附带数据
    strncpy(icmp.magic, MAGIC, MAGIC_LEN);

    // 打上时间戳
    icmp.sending_ts = get_timestamp();

    // 计算校验和
    icmp.checksum = htons(
        calculate_checksum((unsigned char *)&icmp, sizeof(icmp)));

    int bytes = sendto(sock, &icmp, sizeof(icmp), 0,
                       (struct sockaddr *)addr, sizeof(*addr));
    if (bytes == -1)
    {
        return -1;
    }

    return 0;
}

int recv_echo_reply(int sock, int ident)
{

    unsigned char buffer[IP_BUFFER_SIZE];
    struct sockaddr_in peer_addr;

    int addr_len = sizeof(peer_addr);
    int bytes = recvfrom(sock, buffer, sizeof(buffer), 0,
                         (struct sockaddr *)&peer_addr, &addr_len);
    if (bytes == -1)
    {

        if (errno == EAGAIN || errno == EWOULDBLOCK) // 超时处理
        {
            return 0;
        }

        return -1;
    }

    int ip_header_len = (buffer[0] & 0xf) << 2;

    // 解析ICMP回复
    struct icmp_echo *icmp = (struct icmp_echo *)(buffer + ip_header_len);

    // 检查类型
    if (icmp->type != 0 || icmp->code != 0)
    {
        return 0;
    }

    // 检验身份是否一致
    if (ntohs(icmp->ident) != ident)
    {
        return 0;
    }

    // 打印回复
    printf("%s seq=%-5d %8.2fms\n",
           inet_ntoa(peer_addr.sin_addr),
           ntohs(icmp->seq),
           (get_timestamp() - icmp->sending_ts) * 1000);

    return 0;
}

int ping(const char *ip)
{

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));

    // 填充目标地址，端口置0
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    if (inet_aton(ip, (struct in_addr *)&addr.sin_addr.s_addr) == 0)
    {
        fprintf(stderr, "bad ip address: %s\n", ip);
        return -1;
    };

    // 创建原始套接字协议设为ICMP
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock == -1)
    {
        perror("create raw socket");
        return -1;
    }

    // 设置接收超时时间
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = RECV_TIMEOUT_USEC;
    int ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (ret == -1)
    {
        perror("set socket option");
        close(sock);
        return -1;
    }

    double next_ts = get_timestamp();
    int ident = getpid();
    int seq = 1;

    for (;;)
    {

        double current_ts = get_timestamp(); // 间隔发送
        if (current_ts >= next_ts)
        {
            ret = send_echo_request(sock, &addr, ident, seq);
            if (ret == -1)
            {
                perror("Send failed");
            }

            next_ts = current_ts + 1;
            seq += 1;
        }

        ret = recv_echo_reply(sock, ident);
        if (ret == -1)
        {
            perror("Receive failed");
        }
    }

    close(sock);

    return 0;
}

int main(int argc, const char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "no host specified");
        return -1;
    }
    return ping(argv[1]);
}