#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main()
{
    int client_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // 创建客户端套接字
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1)
    {
        perror("socket");
        exit(1);
    }

    // 设置服务器地址和端口
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.2"); // 服务器 IP 地址
    server_addr.sin_port = htons(8080);                   // 服务器端口

    // 连接到服务器
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("connect");
        exit(1);
    }

    printf("Connected to server\n");

    while (1)
    {
        // // 从标准输入读取数据
        // printf("Enter message: ");
        // fgets(buffer, BUFFER_SIZE, stdin);

        // // 发送数据到服务器
        // if (write(client_fd, buffer, strlen(buffer)) == -1)
        // {
        //     perror("write");
        //     exit(1);
        // }

        // // 接收服务器回复的数据
        // int bytes_read = read(client_fd, buffer, BUFFER_SIZE);
        // if (bytes_read == -1)
        // {
        //     perror("read");
        //     exit(1);
        // }
        // else if (bytes_read == 0)
        // {
        //     // 服务器关闭连接
        //     printf("Connection closed by server\n");
        //     break;
        // }
        // else
        // {
        //     // 打印接收到的数据
        //     printf("Received from server: %s\n", buffer);
        // }
    }

    // 关闭客户端套接字
    close(client_fd);

    return 0;
}
