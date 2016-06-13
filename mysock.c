#include <stdlib.h> // getenv()
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h> // timeval
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h> // inet_aton()
#include <sys/un.h> // unix sockets
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "mysock.h"
#include "errlib.h"

/*Both readn and writen look for the error INTERRUPTED_BY_SIGNAL (the system call was interrupted
 * by a caught signal) and continue reading or writing if the error occurs. We handle the error here, 
 * instead of forcing the caller to perform readn or writen again, since the purpose of this functions
 * is to prevent the caller from having to handle a short count.*/
 
#ifndef MAXLINE
#define MAXLINE 1024
#endif

extern char *prog_name;

void Connect (int sockfd, const struct sockaddr *srvaddr, socklen_t addrlen)
{
	if ( connect(sockfd, srvaddr, addrlen) != 0)
		err_sys ("(%s) error - connect() failed", prog_name);
}

int mygetline(char *line, size_t maxline, char *prompt)
{
	char	ch;
	size_t 	i;

	printf("%s", prompt);
	for (i=0; i< maxline-1 && (ch = getchar()) != '\n' && ch != EOF; i++)
		*line++ = ch;
	*line = '\0';
	while (ch != '\n' && ch != EOF)
		ch = getchar();
	if (ch == EOF)
		return(EOF);
	else    return(1);
}

/*Read n bytes from a descriptor*/
ssize_t readn(int fd, void* vptr, size_t n){
	size_t nleft;
	ssize_t nread;
	char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0)
	{
		if ( (nread = read(fd, ptr, nleft)) < 0)
		{
			if (errno == EINTR)
			{
				nread = 0;
				continue; /* and call read() again */
			}
			else
				return -1;
		}
		else
			if (nread == 0)
				break; /* EOF */

		nleft -= nread;
		ptr   += nread;
	}
	return n - nleft;
}

/*Write n bytes to a descriptor*/
ssize_t writen (int fd, const void *vptr, size_t n)
{
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0)
	{
		if ( (nwritten = write(fd, ptr, nleft)) <= 0)
		{
			if (errno == EINTR)
			{
				nwritten = 0;
				continue; /* and call write() again */
			}
			else
				return -1;
		}
		nleft -= nwritten;
		ptr   += nwritten;
	}
	return n;
}

/* read a whole buffer, for performance, and then return one char at a time */
static ssize_t my_read (int fd, char *ptr)
{
	static int read_cnt = 0;	/*static value, so it's shared*/
	static char *read_ptr;
	static char read_buf[MAXLINE];

	if (read_cnt <= 0)		/*it is equal to 0 only at first call, then it  is equal to the value returned by
							the read when it's called and progressively decremented.*/
	{
again:
		if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0)
		{
			if (errno == EINTR)
				goto again;
			return -1;
		}
		else
			if (read_cnt == 0)
				return 0;
		read_ptr = read_buf;
	}
	read_cnt--;		
	*ptr = *read_ptr++;
	return 1;
}

/* NB: Use my_read (buffered recv from stream socket) to get data. Subsequent readn() calls will not behave as expected */
ssize_t readline (int fd, void *vptr, size_t maxlen)
{
	int n, rc;
	char c, *ptr;

	ptr = vptr;
	for (n=1; n<maxlen; n++)
	{
		if ( (rc = my_read(fd,&c)) == 1)
		{
			*ptr++ = c;
			if (c == '\n')
				break;	/* newline is stored, like fgets() */
		}
		else if (rc == 0)
		{
			if (n == 1)
				return 0; /* EOF, no data read */
			else
				break; /* EOF, some data was read */
		}
		else
			return -1; /* error, errno set by read() */
	}
	*ptr = 0; /* null terminate like fgets() */
	return n;
}

ssize_t readline_unbuffered (int fd, void *vptr, size_t maxlen)
{
	int n, rc;
	char c, *ptr;

	ptr = vptr;
	for (n=1; n<maxlen; n++)
	{
		if ( (rc = recv(fd,&c,1,0)) == 1)
		{
			*ptr++ = c;
			if (c == '\n')
				break;	/* newline is stored, like fgets() */
		}
		else if (rc == 0)
		{
			if (n == 1)
				return 0; /* EOF, no data read */
			else
				break; /* EOF, some data was read */
		}
		else
			return -1; /* error, errno set by read() */
	}
	*ptr = 0; /* null terminate like fgets() */
	return n;
}

/*We allow the environment variable LISTENQ to override the value specified by the caller.
 * The problem here is how to properly specify this value.
 * In fact, defining this value as a constant would imply the necessity to recompile the server
 * in case this value needs to be modified.*/
void Listen (int sockfd, int backlog)
{
	char *ptr;
	if ( (ptr = getenv("LISTENQ")) != NULL)
		backlog = atoi(ptr);
	if ( listen(sockfd,backlog) < 0 )
		err_sys ("(%s) error - listen() failed", prog_name);
}

int Socket (int family, int type, int protocol)
{
	int n;
	if ( (n = socket(family,type,protocol)) < 0)
		err_sys ("(%s) error - socket() failed", prog_name);
	return n;
}

void Inet_aton (const char *strptr, struct in_addr *addrptr) {

	if (inet_aton(strptr, addrptr) == 0)
		err_quit ("(%s) error - inet_aton() failed for '%s'", prog_name, strptr);
}

void Bind (int sockfd, const struct sockaddr *myaddr,  socklen_t myaddrlen)
{
	if ( bind(sockfd, myaddr, myaddrlen) != 0)
		err_sys ("(%s) error - bind() failed", prog_name);
}

void Close (int fd)
{
	if (close(fd) != 0)
		err_sys ("(%s) error - close() failed", prog_name);
}
