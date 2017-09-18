#define _GNU_SOURCE 1                    ///因为sockfd需要注册POLLRDHUP事件
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#define BUFFER_SIZE 64

int main(int argc, char ** argv)
{
    if(argc <= 2)
    {
        printf("usage : %s ip port\n",basename(argv[0]));
        return 1;
    }

    const char * ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET,ip,&address.sin_addr);

    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    assert(sockfd >= 0);

    if(connect(sockfd,(struct sockaddr *)&address,sizeof(address)) < 0)
    {
        printf("connection failed\n");
        close(sockfd);
        return 1;
    }

    pollfd fds[2];
    fds[0].fd = 0;                                     ///标准输入
    fds[0].events = POLLIN;                            ///关注可读事件
    fds[0].revents = 0;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP;                 ///注册sockfd上的可读事件，已经监听对方断开连接的事件。
    fds[1].revents = 0;

    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret = pipe(pipefd);             ///待会用户splice函数，将标准输入重定向到网络连接
    assert(ret != -1);

    while(1)
    {
        ret = poll(fds,2,-1);           ///使用poll函数，监听两个文件描述符事件
        if(ret < 0)
        {
            printf("poll failure\n");
            break;
        }

        if(fds[1].revents & POLLRDHUP)    ///POLLRDHUP事件发生
        {
            printf("server close the connection\n");
            break;
        }
        else if(fds[1].revents & POLLIN)
        {
            memset(read_buf,'\0',sizeof(read_buf));
            recv(fds[1].fd,read_buf,BUFFER_SIZE-1,0);
            printf("%s\n",read_buf);
        }

        if(fds[0].revents & POLLIN)              ///标准输入的可读事件
        {
            ///1为管道的写端，我们将标准输入的内容直接拷贝到管道的写端口
            ret = splice(0,NULL,pipefd[1],NULL,32768,SPLICE_F_MORE | SPLICE_F_MOVE);
            ///2为管道的读端口，我们直接将管道里的内容复制给sockf并直接被发送出去
            ret = splice(pipefd[0],NULL,sockfd,NULL,32768,SPLICE_F_MORE | SPLICE_F_MOVE);
            printf("succeed to send message\n");
        }
    }

    close(sockfd);
    return 0;
}
