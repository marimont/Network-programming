/*
 *  TEMPLATE
 */
#include "src/myunp.h"
#include "src/mysock.h"
#include "src/errlib.h"
#include "src/mytcp.h"
#include "src/mystartup.h"
#include "src/utils.h"


#define BLOCKSIZE 1048576

char *prog_name;

int keepalive = 1;

static void connection_closed(int signo){
	fprintf(stdout, "(%s) - connection closed by the client\n", prog_name);
	keepalive = 0; 
	signal(SIGPIPE, connection_closed);
}


int main (int argc, char *argv[]){	
	struct sockaddr_in cliaddr;
	socklen_t addrlen;
	int listenfd, connfd;
	int N;
	uint32_t hash;
	char recvblock[BLOCKSIZE];

	prog_name = argv[0];

	if(argc < 3){
		err_quit("(%s) --- invalid number of input parameters\n", prog_name);
	}
	
	signal(SIGPIPE, connection_closed);
	listenfd = Tcp_listen("0.0.0.0", argv[1], NULL);
	while(1){
		err_msg("(%s) --- waiting for connections on 0.0.0.0:%s\n", prog_name, argv[1]);
		bzero(&cliaddr, sizeof(cliaddr));
		addrlen = sizeof(cliaddr);
		if( (connfd = myAccept(listenfd, (SA*)&cliaddr, &addrlen)) < 0 ){
			break;
		}
		N = atoi(argv[2]);	
		hash = 1;
		while(N){
			bzero(recvblock, BLOCKSIZE);
			if(readn(connfd, recvblock, BLOCKSIZE) <= 0){
				myClose(connfd); break;
			}
			hash =  hashCode(recvblock, BLOCKSIZE, hash);
		N--;
		}

		if( SendUNumber(connfd, hash) != sizeof(uint32_t)){
			err_msg("(%s) --- error sending hash code\n", prog_name);
		} 
		err_msg("(%s) --- hash code sent: %u\n", prog_name, hash);

		myClose(connfd);

	}

	myClose(listenfd);
	return 0;
}
