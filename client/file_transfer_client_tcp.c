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

	/*Assuming to have only relative filenames (i.e. located in the same dir), UNIX limits the filename length to 255.
	*So the length of the buffer that will be sent by the client cannot exceeds 261 chars (255+6)
	* */
#define MAXBUFL 261
#define ERRMSG "-ERR\r\n"
#define OKMSG "+OK\r\n"
#define QUITREQ "QUIT\r\n"
#define msg "Insert a file name of EOF (^D) to exit: "

char *prog_name;

int main(int argc, char** argv){
	int sockfd, nread, nwrite, fd;
	struct sockaddr_in servaddr;
	uint16_t servport;
	char *recvfile, filename[MAXBUFL], answ[6], sendline[MAXBUFL];
	uint32_t size, timestamp;
	
	prog_name = argv[0];
	
	if(argc < 3){
		fprintf(stderr, "(%s) --- wrong number of input parameters\n", prog_name); return 1;
	}
	
	sockfd = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	fprintf(stdout, "(%s) --- socket created\n", prog_name);
	servport = atoi(argv[2]);
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(servport);
	Inet_aton(argv[1], &servaddr.sin_addr);
	
	Connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
	fprintf(stdout, "(%s) --- connected to %s:%s\n", prog_name, argv[1], argv[2]);
	bzero(&filename, sizeof(filename));
	while(mygetline(filename, MAXBUFL, msg) != EOF){
		bzero(&sendline, sizeof(sendline));
		sprintf(sendline, "GET %s\r\n", filename); 
		nwrite = writen(sockfd, sendline, strlen(sendline));
		if(nwrite != strlen(sendline)){
			fprintf(stderr, "(%s) --- can't send GET command\n", prog_name); continue;
		}
		fprintf(stdout, "(%s) --- sent command '%s'\n", prog_name, sendline);
		
		bzero(&answ, sizeof(answ));
		if( (nread = readline_unbuffered(sockfd, answ, 8)) < 0){
			fprintf(stderr, "(%s) --- can't perform readline() - %s\n", prog_name, strerror(errno)); continue;
		} else if(nread == 0){
			fprintf(stderr, "(%s) --- connection closed by the server\n", prog_name); break;
		}
		fprintf(stdout, "(%s) --- received answer '%s'\n", prog_name, answ);
		if(!strcmp(answ, "-ERR\r\n")){
			bzero(&filename, sizeof(filename));
			continue;
		}
		
		if( (nread = readn(sockfd, &size, sizeof(size))) < 0){
			fprintf(stderr, "(%s) --- can't perform read() - %s\n", prog_name, strerror(errno)); continue;
		} else if(nread == 0){
			fprintf(stderr, "(%s) --- connection closed by the server\n", prog_name); break;
		}
		
		uint32_t size_32 =ntohl(size);
		fprintf(stdout, "(%s) --- received size: %u\n", prog_name, size_32);
		
		if( (nread = readn(sockfd, &timestamp, sizeof(timestamp))) < 0){
			fprintf(stderr, "(%s) --- can't perform read() - %s\n", prog_name, strerror(errno)); continue;
		} else if(nread == 0){
			fprintf(stderr, "(%s) --- connection closed by the server\n", prog_name); break;
		}
		
		uint32_t timestamp_32 =ntohl(timestamp);
		fprintf(stdout, "(%s) --- received timestamp: %u\n", prog_name, timestamp_32);
		
		fd = open(filename, O_WRONLY | O_CREAT);
		
		recvfile = (char*) calloc(size_32, sizeof(char));
		nread = readn(sockfd, recvfile, size_32);
		if(nread < 0){
			fprintf(stderr, "(%s) - error. Unable to read from socket - %s\n", prog_name, strerror(errno)); continue;
		} else if( nread == 0 || nread != size_32){
			fprintf(stderr, "(%s) - connection closed by the server\n", prog_name); 
			close(fd);
			break;
		} 
		
		nwrite = write(fd, recvfile, nread);
		if(nwrite != nread ){
			fprintf(stderr, "(%s) - incomplete file\n", prog_name); 
		}else{
			fprintf(stdout, "(%s) --- '%s' has been received and stored\n", prog_name, filename);
		}	
		
		close(fd);
		bzero(&filename, sizeof(filename));
	}
	
	Close(sockfd);
	return 0;
	
	
}






















