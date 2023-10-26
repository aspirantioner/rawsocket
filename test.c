#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

int main()
{
    int server_fd, client_fd, epoll_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    struct epoll_event event, events[MAX_EVENTS];
    char buffer[BUFFER_SIZE];

    // 创建服务器套接字
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket");
        exit(1);
    }

    // 设置服务器地址和端口
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.2");
    server_addr.sin_port = htons(8080);

    // 绑定服务器套接字到指定地址和端口
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
        exit(1);
    }

    // 监听连接请求
    if (listen(server_fd, 10) == -1)
    {
        perror("listen");
        exit(1);
    }

    // 创建 epoll 实例
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        perror("epoll_create1");
        exit(1);
    }

    // 添加服务器套接字到 epoll 实例
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
    {
        perror("epoll_ctl");
        exit(1);
    }

    printf("Server started. Listening on port 8080...\n");

    while (1)
    {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1)
        {
            perror("epoll_wait");
            exit(1);
        }

        for (int i = 0; i < num_events; i++)
        {
            if (events[i].data.fd == server_fd)
            {
                // 有新的连接请求
                client_addr_len = sizeof(client_addr);
                client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

                if (client_fd == -1)
                {
                    perror("accept");
                    exit(1);
                }

                // set socket timeout option
                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 100;
                int ret = setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                if (ret == -1)
                {
                    perror("set time out error");
                    exit(0);
                }

                // 将新的客户端套接字添加到 epoll 实例
                event.events = EPOLLIN|EPOLLERR;
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1)
                {
                    perror("epoll_ctl");
                    exit(1);
                }

                printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            }
            else
            {
                // 有数据可读
                int bytes_read = read(events[i].data.fd, buffer, BUFFER_SIZE);
                if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
                {
                    perror("read time out");
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                    printf("Connection closed by client\n");
                    close(events[i].data.fd);
                }
                else if (bytes_read == 0)
                {
                    // 客户端关闭连接
                    printf("Connection closed by client\n");
                    close(events[i].data.fd);
                }
                else
                {
                    // 处理接收到的数据
                    printf("Received data from client: %s\n", buffer);

                    // 回复相同的数据给客户端
                    write(events[i].data.fd, buffer, bytes_read);
                }
            }
        }
    }

    // 关闭服务器套接字和 epoll 实例
    close(server_fd);
    close(epoll_fd);

    return 0;
}
