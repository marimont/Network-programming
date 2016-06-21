#include "src/myunp.h"
#include "src/mysock.h"
#include "src/mystartup.h"
#include "src/mytcp.h"
#include "src/myutils.h"
#include "src/errlib.h"

#define MAXBUFL 25
#define OKMSG "+OK\r\n"
#define ERRMSG "-ERR\r\n"

char *prog_name;
int keepalive = 1;

int main(int argc, char* argv[]){
	int listenfd, connfd;
	char sendline[MAXBUFL+1], recvline[MAXBUFL+1], filename[MAXBUFL+1];
	struct sockaddr_in cliaddr;
	socklen_t addrlen;

	prog_name = argv[0];

	if(argc != 2){
		err_quit("(%s) --- invalid number of parameters\n", prog_name);
	}

	listenfd = Tcp_listen("0.0.0.0", argv[1], NULL);
	while(1){
		fprintf(stdout, "(%s) --- waiting for connections on 0.0.0.0:%s\n", prog_name, argv[1]);
		bzero(&cliaddr, sizeof(cliaddr));
		addrlen = sizeof(cliaddr);
		if( (connfd = myAccept(listenfd, (SA*)&cliaddr, &addrlen)) < 0 ){
			continue;
		}
		fprintf(stdout, "(%s) --- connection estabilished with %s. Waiting for file\n", prog_name, Sock_ntop((SA*)&cliaddr, addrlen));
		if(myReadline_unbuffered(connfd, recvline, 255) > MAXBUFL+1){
			err_msg("(%s) --- invalid string. Sending ERR msg\n", prog_name);
			sprintf(sendline, "%s", ERRMSG);
			myWriten(connfd, sendline, strlen(sendline));
			myClose(connfd);
			continue;
		}	
			
		/*correct format, accept file*/
		sprintf(sendline, "%s", OKMSG);
		if(myWriten(connfd, sendline, strlen(OKMSG)) < 0){
			myClose(connfd);
			continue;
		}
		fprintf(stdout, "(%s) --- OK msg sent\n", prog_name);

	
		strcpy(filename, recvline+3);
		filename[strlen(filename) - strlen("\r\n")] = '\0';
		if(myTcpReadUnbufferedAndWriteToFile(connfd, filename) < 0){
			err_msg("(%s) --- reception failed\n)", prog_name);
			myClose(connfd);
			continue;
		}
		fprintf(stdout, "(%s) --- file received\n", prog_name);
	}

	myClose(listenfd);
	return 0;
}

















