#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <fcntl.h>

#define USER_LIMIT 5      /* 最大用户数量           */
#define BUFFER_SIZE 64    /* 读缓冲区的大小         */
#define FD_LIMIT   65535  /* 文件描述符数量的限制    */



/**
 *  客户端socket地址、待写到客户端的数据的位置、从客户端读入的数据
 */
struct client_data
{
    struct sockaddr_in address;
    char * write_buf;
    char buf[BUFFER_SIZE];
};

int setnonblocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

int main(int argc, char ** argv)
{
    if(argc <= 2)
    {
        printf("usage : %s ip port\n",basename(argv[0]));
        return 1;
    }

    const char * ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in  address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET,ip,&address.sin_addr);

    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    assert(listenfd >= 0);

    ret = bind(listenfd,(struct sockaddr *)&address,sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd,5);
    assert(ret != -1);

    ///创建users数组，分配FD_LIMIT个client_data对象，可以预期：
    ///每个可能的socket连接都可以获得一个这样的对象，并且socket的值可以直接用了索引socket对应的client_data对象。
    client_data * users = new client_data[FD_LIMIT];

    ///尽管我们分配足够多client_data对象了，为了提高poll的性能，还是需要限制用户的数量
    pollfd fds[USER_LIMIT + 1];
    int user_counter = 0;
    for(int i = 1;i <= USER_LIMIT; i++)
    {
        fds[i].fd = -1;
        fds[i].events = 0;
    }

    ///fds[0]用于监听listenfd文件描述符
    fds[0].fd = listenfd;
    fds[0].events = POLLIN | POLLERR;    ///需要监听的事件包括：文件描述符的可读事件和错误事件
    fds[0].revents = 0;

    while(1)
    {
        ret = poll(fds,user_counter + 1, -1);      ///调用poll函数，注册需要监听的文件描述符
        if(ret < 0)
        {
            printf("poll failure\n");
            break;
        }

        for(int i = 0; i < user_counter + 1; i++)     ///遍历所有的文件描述符的状态
        {
            if((fds[i].fd ==  listenfd) && (fds[i].revents & POLLIN))  ///这里代表有新的连接过来了
            {
                struct sockaddr_in client;
                socklen_t c_len;
                int connfd = accept(listenfd,(struct sockaddr*)&client,&c_len);
                if(connfd < 0)
                {
                    printf("errno is %d\n",errno);
                    continue;
                }
                if(user_counter >= USER_LIMIT)
                {
                    const char * info = "too many users\n";
                    printf("%s",info);
                    send(connfd,info,strlen(info),0);
                    close(connfd);
                    continue;
                }

                ///对于新的连接，同时修改fds和users数组，前文已经提到users[connfd]对应于新的文件描述符connfd的数据
                user_counter++;
                users[connfd].address = client;
                setnonblocking(connfd);
                fds[user_counter].fd = connfd;
                fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_counter].revents = 0;
                printf("comes a new user,now have %d users\n",user_counter);
            }
            else if(fds[i].revents & POLLERR)
            {
                printf("get an error from %d\n",fds[i].fd);
                char errors[100];
                memset(errors,'\0',sizeof(errors));
                socklen_t length = sizeof(errors);
                ///SO_ERROR：获取并清除socket错误状态
                if(getsockopt(fds[i].fd,SOL_SOCKET,SO_ERROR,&errors,&length) < 0)
                {
                    printf("get socket option failed\n");

                }
                continue;
            }
            else if(fds[i].revents & POLLRDHUP)
            {
                users[fds[i].fd] = users[fds[user_counter].fd];  ///这个语句不明白其作用，看不懂（或许只是一个数据填充）
                close(fds[i].fd);
                fds[i] = fds[user_counter];
                i--;
                user_counter--;
                printf("a client left\n");
            }
            else if(fds[i].revents & POLLIN)
            {

                printf("enter the POLLIN\n");
                int connfd = fds[i].fd;
                memset(users[connfd].buf,'\0',BUFFER_SIZE);
                ret = recv(connfd,users[connfd].buf,BUFFER_SIZE - 1, 0);
                if(ret < 0 )
                {
                    if(errno != EAGAIN)
                    {
                        close(connfd);
                        users[fds[i].fd] = users[fds[user_counter].fd];
                        fds[i] = fds[user_counter];
                        i--;
                        user_counter--;
                    }
                }
                else if(ret == 0)
                {
                }
                else
                {
                    printf("get %d bytes of client data %s frome %d\n",ret,users[connfd].buf,connfd);

                    for(int j = 1; j <= user_counter; ++j)
                    {
                        if(fds[j].fd == connfd)
                        {
                            continue;
                        }
                        fds[j].events |= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].write_buf = users[connfd].buf;
                    }
                }

            }
            else if(fds[i].revents & POLLOUT)
            {
                int connfd = fds[i].fd;
                if(! users[connfd].write_buf){
                    continue;
                }

                ret = send(connfd,users[connfd].write_buf,strlen(users[connfd].write_buf),0);
                users[connfd].write_buf = NULL;
                ///写完数据，重新注册fds[i]的可读事件
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
        }
    }
    delete [] users;
    close(listenfd);
    return 0;
}
