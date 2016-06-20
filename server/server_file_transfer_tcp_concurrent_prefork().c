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
#include <sys/wait.h>

#define LISTENQ 15
#define BLOCKSIZE 1048576
	/*Assuming to have only relative filenames (i.e. located in the same dir), UNIX limits the filename length to 255.
	*So the length of the buffer that will be sent by the client cannot exceeds 261 chars (255+6)
	* */
#define MAXBUFL 261
#define ERRMSG "-ERR\r\n"
#define OKMSG "+OK\r\n"
#define QUITREQ "QUIT\r\n"
#define CHILDN 3

char *prog_name;
pid_t childpid[CHILDN];

int keepalive, nservices = 0;

int min(int x, int y);

static void sig_int(int signo) {
	int i;
	err_msg ("(%s) info - sig_int() called", prog_name);
	for (i=0; i<CHILDN; i++)
		kill(childpid[i], SIGTERM);

	while( wait(NULL) > 0)  // wait for all children
		;

	if (errno != ECHILD)
		err_quit("(%s) error: wait() error");

	exit(0);
}


static void sign_handler(int signo){
	if(signo == SIGPIPE){
		fprintf(stdout, "(%s) - connection closed by the client\n", prog_name);
		keepalive = 0; 	
		signal(SIGPIPE, sign_handler);	//reinstall handler
	} 
}


void service(int listenfd, pid_t pid, char *servaddr, char *servport){
	int n, nwrite, fd, connfd;
	char block[BLOCKSIZE], recvline[MAXBUFL], filename[MAXBUFL], command[4];
	uint32_t size, timestamp;
	struct sockaddr_in cliaddr;
	socklen_t addrlen;
	
	signal(SIGPIPE, sign_handler);
	
	fprintf(stdout, "(PID = %d) --- child process started\n", pid);
	
	while(1){
		fprintf(stdout, "(%d) --- waiting for connections on %s:%s\n", pid, servaddr, servport); fflush(stdout);
		bzero(&cliaddr, sizeof(cliaddr));
		addrlen = sizeof(cliaddr);
		if( (connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &addrlen)) < 0 ){
			if(connfd == EINTR)
				continue;
			else{
				fprintf(stderr, "(%d) --- accept() failed - %s\n", pid, strerror(errno)); continue;
			}
		}
		fprintf(stdout, "(%d) --- connection with %s:%hu estabilished\n", pid, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
		keepalive = 1;
		/*Wait for client requests*/
		while(keepalive){
			fprintf(stdout, "(PID = %d) --- Waiting for commands...\n", pid); fflush(stdout);	
			bzero(&recvline, sizeof(recvline));
			if((n = readline_unbuffered(connfd, recvline, MAXBUFL)) < 0){
				fprintf(stderr, "(PID = %d) --- readline() failed - %s\n", pid, strerror(errno)); continue;
			} else if(n == 0){
				fprintf(stderr, "(PID = %d) --- connection closed by third party\n", pid); break;
			}
			fprintf(stdout, "(PID = %d) --- received command '%s'\n", pid, recvline);
			if(!strcmp(recvline, QUITREQ)){
			/* -QUIT msg received*/
				if (close(connfd) != 0){
					fprintf(stderr, "(PID = %d) --- connected socket close() failed\n", pid);
				}
				break;	//break the single-client loop -> wait for new connections
			}
		
			/*check on request format correctness*/
			bzero(&command, sizeof(command));
			bzero(&filename, sizeof(filename));
			if(sscanf(recvline, "%s %s", command, filename) != 2 || strcmp(command, "GET")){
				fprintf(stderr, "(PID = %d) - can't serve request: wrong format\n", pid); continue;
			}
			struct stat buf;
			n = stat(filename, &buf);
			if(n < 0){
				fprintf(stdout, "(PID = %d) - not able to stat: %s\n", pid, strerror(errno));
				nwrite = writen(connfd, ERRMSG, 6);					
				if(nwrite < 1){
					fprintf(stderr, "(PID = %d) - writing failed\n", pid); continue;
				}
				fprintf(stdout, "(PID = %d) --- sent message '%s'\n", pid, ERRMSG);
				continue;
			}
			size = (uint32_t) buf.st_size;
			timestamp = (uint32_t) buf.st_mtim.tv_sec;
		
			nwrite = writen(connfd, OKMSG, 5);					
			if(nwrite != strlen(OKMSG)){
				fprintf(stderr, "(PID = %d) - writing failed\n", pid); continue;
			}
			fprintf(stdout, "(PID = %d) --- sent message '%s'\n", pid, OKMSG);
		
		
			uint32_t size_32 = htonl(size);
			uint32_t timestamp_32 = htonl(timestamp);
			nwrite = writen(connfd, &size_32, sizeof(size_32));					
			if(nwrite != sizeof(size_32)){
				fprintf(stderr, "(PID = %d) - writing failed\n", pid); continue;
			}
			fprintf(stdout, "(PID = %d) --- sent size '%u'\n", pid, size);
			nwrite = writen(connfd, &timestamp_32, sizeof(timestamp_32));					
			if(nwrite != sizeof(timestamp_32)){
				fprintf(stderr, "(PID = %d) - writing failed\n", pid); continue;
			}
			fprintf(stdout, "(PID = %d) --- sent timestamp '%u'\n", pid, timestamp);
		
			fd = open(filename, O_RDONLY);
			int toBeSent = size;		//at each cycle I subtract the sent part 
			fprintf(stdout, "(PID = %d) --- trasmission of file '%s' starting...\n", pid, filename);
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
				fprintf(stdout, "(PID = %d) --- trasmission complete\n", pid);
			} else {
				fprintf(stdout, "(PID = %d) --- trasmission failed\n", pid);
			}
			close(fd);	
		}
		
		if(close(connfd) < 0){
			fprintf(stdout, "(PID = %d) --- close() failed - %s\n", pid, strerror(errno));
		}
	}
	
	exit(0);
}	

int main(int argc, char** argv){
	int listenfd, i, sigact_res;
	struct sockaddr_in servaddr;
	uint16_t servport;
	struct sigaction action;
	
	prog_name = argv[0];
	
	if(argc < 2){
		fprintf(stderr, "(%s) --- wrong number of input parameters\n", prog_name); return 1;
	}
	
	signal(SIGCHLD, sign_handler);
	
	listenfd = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	fprintf(stdout, "(%s) --- socket created\n", prog_name);
	
	servport = atoi(argv[1]);
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(servport);
	
	Bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

	Listen(listenfd, LISTENQ);
	for(i = 0; i < CHILDN; i++){
		if( (childpid[i] = fork()) == 0){
			/*child process*/
			service(listenfd, getpid(), inet_ntoa(servaddr.sin_addr), argv[1]);
		}
		
		/*father process: keeps on creating child processes then dies*/
	}
	
	
	/* This is to capture CTRL-C and terminate children 
	 * Otherwise all children remain alive in the OS!!!*/	
	memset(&action, 0, sizeof (action));
	action.sa_handler = sig_int;
	sigact_res = sigaction(SIGINT, &action, NULL);
	if (sigact_res == -1)
		err_quit("(%s) sigaction() failed", prog_name);


	/*the father is paused waiting for a ^c*/
	while(1) {
		pause();
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












