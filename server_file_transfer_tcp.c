#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>

#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "mysock.h"
#include "errlib.h"
#include <fcntl.h>

#include <signal.h>
#include <sys/stat.h>

#define LISTENQ 15
#define BLOCKSIZE 1048576
	/*Assuming to have only relative filenames (i.e. located in the same dir), UNIX limits the filename length to 255.
	*So the length of the buffer that will be sent by the client cannot exceeds 261 chars (255+6)
	* */
#define MAXBUFL 261
#define ERRMSG "-ERR\r\n"
#define OKMSG "+OK\r\n"
#define QUITREQ "QUIT\r\n"

char *prog_name;

int keepalive;

int min(int x, int y);

static void connection_closed(int signo){
	fprintf(stdout, "(%s) - connection closed by the client\n", prog_name);
	keepalive = 0; 
}

int main(int argc, char** argv){
	int listenfd, connfd, n, nwrite, fd;
	struct sockaddr_in servaddr, cliaddr;
	uint16_t servport;
	char block[BLOCKSIZE], recvline[MAXBUFL], filename[MAXBUFL], command[4];
	socklen_t addrlen;
	uint32_t size, timestamp;
	
	prog_name = argv[0];
	
	if(argc < 2){
		fprintf(stderr, "(%s) --- Error: invalid number of parameters. You should provide a port number\n", prog_name);
		return 1;
	}
	
	listenfd = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	fprintf(stdout, "(%s) --- socket created\n", prog_name);
	
	servport = atoi(argv[1]);
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(servport);
	
	Bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
	
	Listen(listenfd, LISTENQ);	
	signal(SIGPIPE, connection_closed);
	while(1){
		fprintf(stdout, "(%s) --- waiting for connections on %s:%s\n", prog_name, inet_ntoa(servaddr.sin_addr), argv[1]);
		addrlen = sizeof(cliaddr);
		bzero(&cliaddr, sizeof(cliaddr));
		connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &addrlen);
		if(connfd < 0){
			fprintf(stderr, "(%s) --- accept() failed - %s\n", prog_name, strerror(errno)); continue;
		}
		fprintf(stdout, "(%s) --- connection estabilished with client at %s:%u\n", prog_name, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
		
		keepalive = 1;
		/*Wait for client requests*/
		while(keepalive){
			fprintf(stdout, "(%s) --- Waiting for commands...\n", prog_name); fflush(stdout);	
			bzero(&recvline, sizeof(recvline));
			if((n = readline_unbuffered(connfd, recvline, MAXBUFL)) < 0){
				fprintf(stderr, "(%s) --- readline() failed - %s\n", prog_name, strerror(errno)); continue;
			} else if(n == 0){
				fprintf(stderr, "(%s) --- connection closed by third party\n", prog_name); break;
			}
			fprintf(stdout, "(%s) --- received command '%s'\n", prog_name, recvline);
			if(!strcmp(recvline, QUITREQ)){
			/* -QUIT msg received*/
				if (close(connfd) != 0){
					fprintf(stderr, "(%s) --- connected socket close() failed\n", prog_name);
				}
				break;	//break the single-client loop -> wait for new connections
			}
			
			/*check on request format correctness*/
			bzero(&command, sizeof(command));
			bzero(&filename, sizeof(filename));
			if(sscanf(recvline, "%s %s", command, filename) != 2 || strcmp(command, "GET")){
				fprintf(stderr, "(%s) - can't serve request: wrong format\n", prog_name); continue;
			}
			struct stat buf;
			n = stat(filename, &buf);
			if(n < 0){
				fprintf(stdout, "(%s) - not able to stat: %s\n", prog_name, strerror(errno));
				nwrite = writen(connfd, ERRMSG, 6);					
				if(nwrite < 1){
					fprintf(stderr, "(%s) - writing failed\n", prog_name); continue;
				}
				fprintf(stdout, "(%s) --- sent message '%s'\n", prog_name, ERRMSG);
				continue;
			}
			size = (uint32_t) buf.st_size;
			timestamp = (uint32_t) buf.st_mtim.tv_sec;
			
			nwrite = writen(connfd, OKMSG, 5);					
			if(nwrite != strlen(OKMSG)){
				fprintf(stderr, "(%s) - writing failed\n", prog_name); continue;
			}
			fprintf(stdout, "(%s) --- sent message '%s'\n", prog_name, OKMSG);
			
			
			uint32_t size_32 = htonl(size);
			uint32_t timestamp_32 = htonl(timestamp);
			nwrite = writen(connfd, &size_32, sizeof(size_32));					
			if(nwrite != sizeof(size_32)){
				fprintf(stderr, "(%s) - writing failed\n", prog_name); continue;
			}
			fprintf(stdout, "(%s) --- sent size '%u'\n", prog_name, size);
			nwrite = writen(connfd, &timestamp_32, sizeof(timestamp_32));					
			if(nwrite != sizeof(timestamp_32)){
				fprintf(stderr, "(%s) - writing failed\n", prog_name); continue;
			}
			fprintf(stdout, "(%s) --- sent timestamp '%u'\n", prog_name, timestamp);
			
			fd = open(filename, O_RDONLY);
			int toBeSent = size;		//at each cycle I subtract the sent part 
			fprintf(stdout, "(%s) --- trasmission of file '%s' starting...\n", prog_name, filename);
			while(toBeSent && keepalive){			//until there is something to be sent
				bzero(&block, sizeof(block));
				/*At each iteration I sent a block: the reason is that sending big chunck of data could be blocking
				 * or not feasible, while sending 1 char at a time is not feasible in terms of time. At each iteration
				 * I'm gonna send or a whole block of 1MB or the remaining part of the file if it is smaller than 1MB*/
				int nread = read(fd, block, min(toBeSent, BLOCKSIZE));		
				nwrite = writen(connfd, block, nread);					
				if(nwrite != nread){
					break;
				}
				toBeSent -= nwrite;	

			}
			if(toBeSent == 0){
				fprintf(stdout, "(%s) --- trasmission complete\n", prog_name);
			} else {
				fprintf(stdout, "(%s) --- trasmission failed\n", prog_name);
			}
			close(fd);
			
		}
	}

	Close(listenfd);
	
	return 0;
}

int min(int x, int y){
	int min = x;
	if(y < x)
		min = y;
	return min;
}

















