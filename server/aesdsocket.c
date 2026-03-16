#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <threads.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <string.h>
#include <netdb.h>

#define PORT 9000

int main()
{
int socketfd, new_fd;//socket file descriptor
int status; //result of gettaddrinfo
struct addrinfo hints; //for getaddrinfo
struct addrinfo *service_info;
struct sockaddr_storage their_addr;
socklen_t addr_size;

//file info



memset(&hints,0,sizeof(hints));//makesure the struct is empty

hints.ai_family = AF_UNSPEC; //either IPV4 or IPV6
hints.ai_socktype = SOCK_STREAM;//tcp stream socket 
hints.ai_flags = AI_PASSIVE;//fill the ip automatically

if(status = getaddrinfo(NULL,9000,&hints,&service_info) != 0) //gettig the addrinfo struct
{
    perror("getaddrinfo");
    exit(-1);
}
// service_info now points to a linked list of 1 or more
// struct addrinfos

socketfd = socket(service_info->ai_family,service_info->ai_socktype,service_info->ai_protocol);//creating the socketfd
if(socketfd  == -1)
{
    perror("socket");
    exit(-1);
}

if(bind(socketfd,service_info->ai_addr,
service_info->ai_addrlen) == -1) //binding the socketfd, seeting up
{
    perror("bind");
    exit(-1);
}

if(connect(socketfd,service_info->ai_addr,service_info->ai_addrlen) == -1) //connecting
{
    perror("connect");
    exit(-1);
}
if(listen(socketfd,5) == -1) // ready to listen
{
    perror("listen");
    extit(-1);
}
addr_size = sizeof their_addr;
    new_fd = accept(socketfd, (struct sockaddr *)&their_addr, &addr_size); //accept queues to the port



return 0;
}