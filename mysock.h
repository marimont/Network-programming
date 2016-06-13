#ifndef _MYSOCK_H
#define _MYSOCK_H
#endif

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>

/*input from stdin*/
int mygetline(char *line, size_t maxline, char *prompt);

/*This function is almost identical to the normal read syscall but it cycles until
 * all the specified bytes are read, EOF is met or an error occurs*/
ssize_t readn(int fd, void* vptr, size_t n);

/*This function is almost identical to the normal write syscall but it cycles until
 * all the specified bytes are written or an error occurs*/
ssize_t writen (int fd, const void *vptr, size_t n);

/*these two functions work like gets: they store also \n\0, so you need to provide length+1 
 * to it in order to be sure that all chars are gonna be read*/

/*buffered input*/
ssize_t readline (int fd, void *vptr, size_t maxlen);

/*Reads until in meets a \n. Slower than buffered readline*/
ssize_t readline_unbuffered (int fd, void *vptr, size_t maxlen);

void Connect (int sockfd, const struct sockaddr *srvaddr, socklen_t addrlen);
void Listen (int sockfd, int backlog);
int Socket (int family, int type, int protocol);
void Inet_aton (const char *strptr, struct in_addr *addrptr);
void Bind (int sockfd, const struct sockaddr *myaddr,  socklen_t myaddrlen);
void Close (int fd);


