#include "src/myunp.h"
#include "src/mysock.h"
#include "src/errlib.h"
#include "src/mystartup.h"
#include "src/mytcp.h"

#define MAXTRIALS 4

#define MAXBUFL 261
#define ERRMSG "-ERR\r\n"
#define OKMSG "+OK\r\n"
#define QUITREQ "QUIT\r\n"
#define msg "Insert a file name of EOF (^D) to exit: "

char *prog_name;
int keepalive = 1;

int main(int argc, char** argv){
	int sockfd, n;
	uint32_t size;
	fd_set rset;
	char recvline[MAXBUFL], sendline[MAXBUFL], filename[MAXBUFL],
		servaddr[MAXBUFL], servport[MAXBUFL];

	prog_name = argv[0];
	
	if(argc < 4){
		err_quit("(%s) --- invalid number of parameters", prog_name);
	}
	n = 0;
	strcpy(servaddr, argv[1]);
	strcpy(servport, argv[2]);
	
	while(n < 4){
		sockfd = Tcp_connect(servaddr, servport);
		strcpy(filename, argv[3]);
		if(!strcmp(filename, "QUIT")){
			if(myWriten(sockfd, QUITREQ, strlen(QUITREQ)) < 0){
				myClose(sockfd); return -1;
			}
			myClose(sockfd); return 0;
		}

		sprintf(sendline, "GET%s\r\n", filename);
		if(myWriten(sockfd, sendline, strlen(sendline)) < 0){
			myClose(sockfd); return -1;
		}
		fprintf(stdout, "(%s) --- send command '%s'\n", prog_name, sendline);
		bzero(recvline, MAXBUFL);
		if(myReadline_unbuffered(sockfd, recvline, MAXBUFL) < 0){
			myClose(sockfd); return -1;
		}
		//bzero(recvline, MAXBUFL);
		//Readline_unbuffered(sockfd, recvline, MAXBUFL);
		fprintf(stdout, "(%s) --- received answer: '%s'\n", prog_name, recvline);
		if(!strcmp(recvline, ERRMSG)){
			myClose(sockfd); return -2;
		} else if(!strcmp(recvline, OKMSG)){
		  	if(myReadn(sockfd, &size, sizeof(size)) < 0){
				myClose(sockfd); return -1;
			}
			size = ntohl(size);
			fprintf(stdout, "(%s) --- received size: '%u'\n", prog_name, size);
			if( myTcpReadChunksAndWriteToFile(sockfd, filename, size, NULL) < 0){
			 	fprintf(stdout, "(%s) --- file transfer failed\n", prog_name); 
			} else
				fprintf(stdout, "(%s) --- file correctly stored\n", prog_name); 
			myClose(sockfd); return 0;
		} else {
			strcpy(recvline, recvline + 3);
			sscanf(recvline, "%s %s", servaddr, servport);
			fprintf(stdout, "(%s) --- redirection to %s:%s\n", prog_name, servaddr, servport);
			n++;
			myClose(sockfd);
		}
	}
	fprintf(stdout, "(%s) --- the required file hasn't been found", prog_name);
	return 0;
}























