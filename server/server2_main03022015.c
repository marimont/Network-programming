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

#define MAXCHILDS 3

char *prog_name;

int keepalive = 1;

pid_t childpid[MAXCHILDS];

static void connection_closed(int signo){

	if(signo == SIGPIPE){
		fprintf(stdout, "(%s) - connection closed by the client\n", prog_name);
		keepalive = 0; 
		signal(SIGPIPE, connection_closed);	
	} else if(signo == SIGCHLD){
		pid_t pid;
		while( (pid = waitpid(-1,NULL, WNOHANG)) > 0 )
			fprintf(stdout, "(%s) --- child process %d terminated\n", prog_name, pid);
		
		signal(SIGCHLD, connection_closed);
	}
}

void service(int connfd, pid_t pid, int N){
	struct timeval tval;
	fd_set rset;
	char recvblock[BLOCKSIZE];
	uint32_t hash;
	int s;
	hash = 1;
		while(N){
			tval.tv_sec = 10;
			tval.tv_usec = 0;
			FD_ZERO(&rset);
			FD_SET(connfd, &rset);
			if( (s = select(FD_SETSIZE, &rset, NULL, NULL, &tval)) < 0){
				/*error*/
				err_msg("(%s) --- select failed\n", prog_name); break;			
			}if(s == 0){
				err_msg("(%s) --- timeout expired\n", prog_name); break;
			}
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

int main (int argc, char *argv[]){	
	struct sockaddr_in cliaddr;
	socklen_t addrlen;
	int listenfd, connfd;
	int N, nservices = 0;
	
	prog_name = argv[0];

	if(argc < 3){
		err_quit("(%s) --- invalid number of input parameters\n", prog_name);
	}
	
	signal(SIGPIPE, connection_closed);
	signal(SIGCHLD, connection_closed);
	listenfd = Tcp_listen("0.0.0.0", argv[1], NULL);
	while(1){
		if(nservices > MAXCHILDS){
			fprintf(stdout, "(%s) --- no more connections can be accepted\n", prog_name); 
			wait(NULL);
			nservices--;
		}
		err_msg("(%s) --- waiting for connections on 0.0.0.0:%s\n", prog_name, argv[1]);
		bzero(&cliaddr, sizeof(cliaddr));
		addrlen = sizeof(cliaddr);
		if( (connfd = myAccept(listenfd, (SA*)&cliaddr, &addrlen)) < 0 ){
			break;
		}
		nservices++;
		N = atoi(argv[2]);	
		if( childpid[nservices] == 0 ){
			/*child process*/
			service(connfd, getpid(), N);
			exit(0);
		}
		myClose(connfd);

	}

	myClose(listenfd);
	return 0;
}
