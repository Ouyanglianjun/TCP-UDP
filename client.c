/*******************************/
/*     可以同时发起TCP和UDP请求    */
/*******************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

int main(int argc, char ** argv)
{
    if(argc <= 3)
    {
        printf("usage : %s  ip_number ip_port type\n",basename(argv[0]));
        return 0;
    }

    const char * ip = argv[1];
    int port = atoi(argv[2]);
    int type = atoi(argv[3]);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET,ip,&address.sin_addr);

    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    assert(sockfd >= 0);

    const char * data = "abcdecfdhijkl";
    if(type == 1) ///tcp
    {

        if(connect(sockfd,(struct sockaddr *)&address,sizeof(address)) < 0)
        {
            printf("connection error\n");
            return 0;
        }
        send(sockfd,data,strlen(data),0);
    }
    else
    {
        bzero(&address,sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        inet_pton(AF_INET,ip,&address.sin_addr);

        sockfd = socket(AF_INET,SOCK_DGRAM,0);
        assert(sockfd >= 0);
        sendto(sockfd,data,strlen(data),0,(struct sockaddr*)&address,sizeof(address));
    }
    close(sockfd);
    return 0;

}
