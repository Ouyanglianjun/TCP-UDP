/************************************/
/*     此服务器程序可以同时处理TCP和UDP  */
/*     请求，通过epoll的I/O复用技术实现的*/
/************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

int setnonblocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void addfd(int epollfd,int fd)
{
     epoll_event event;
     event.data.fd = fd;
     event.events = EPOLLIN | EPOLLET;
     epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
     setnonblocking(fd);
}

int main(int argc, char ** argv)
{
    if(argc <= 2)
    {
        printf("usage : %s ip_address ip_port\n",basename(argv[0]));
        return 1;
    }

    const char * ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET,ip,&address.sin_addr);

    ///创建TCP Socket 并且绑定到端口上
    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    assert(listenfd >= 0);

    ret = bind(listenfd,(struct sockaddr *)&address,sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd,5);
    assert(ret != -1);

    ///创建UDP  socket ，并将其绑定到端口port上
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET,ip,&address.sin_addr);
    int udpfd = socket(AF_INET,SOCK_DGRAM,0);
    assert(udpfd >= 0);

    ret = bind(udpfd,(struct sockaddr *)&address,sizeof(address));
    assert(ret != -1);


    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);

    ///注册TCP socket 和 UDP socket 上的可读事情
    addfd(epollfd,listenfd);
    addfd(epollfd,udpfd);

    while(1)
    {
         int number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
         if(number < 0)
         {
              printf("epoll failurl\n");
              break;
         }

         for(int i = 0; i < number; i++)
         {
              int sockfd = events[i].data.fd;
              if(sockfd == listenfd)
              {
                  struct sockaddr_in client;
                  socklen_t c_len = sizeof(client);
                  int connfd = accept(listenfd,(struct sockaddr*)&client,&c_len);
                  addfd(epollfd,connfd);
              }
              else if(sockfd == udpfd)
              {
                   char buf[UDP_BUFFER_SIZE];
                   memset(buf,'\0',UDP_BUFFER_SIZE);
                   struct sockaddr_in client;
                   socklen_t c_len = sizeof(client);

                   ret = recvfrom(udpfd,buf,UDP_BUFFER_SIZE - 1,0,(struct sockaddr*)&client,&c_len);

                   if(ret > 0)
                   {
                         printf("UDP get content : %s\n",buf);
                         //sendto(udpfd,buf,UDP_BUFFER_SIZE,0,(struct sockaddr*)&client,c_len);
                   }
              }
              else if(events[i].events & EPOLLIN)
              {
                    char buf[TCP_BUFFER_SIZE];
                    while(1)
                    {
                         memset(buf,'\0',TCP_BUFFER_SIZE);
                         ret = recv(sockfd,buf,TCP_BUFFER_SIZE - 1,0);
                         if(ret < 0)
                         {
                              if((errno == EAGAIN) || (errno == EWOULDBLOCK))
                              {
                                   break;
                              }
                              close(sockfd);
                              break;
                         }
                         else if(ret == 0)
                         {
                              close(sockfd);
                         }
                         else
                         {
                              ///send(sockfd,buf,ret,0);
                               printf("TCP get content : %s\n",buf);
                         }
                    }
              }
              else
              {
                    printf("something else happened\n");
              }
         }
    }
    close(listenfd);
    return 0;
}
