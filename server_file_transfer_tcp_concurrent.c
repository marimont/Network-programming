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
#define MAXCHLD 3

char *prog_name;

int keepalive, nservices = 0;

int min(int x, int y);

static void sign_handler(int signo){
	if(signo == SIGPIPE){
		fprintf(stdout, "(%s) - connection closed by the client\n", prog_name);
		keepalive = 0; 	
		signal(SIGPIPE, sign_handler);	//reinstall handler
	} else if(signo == SIGCHLD){
		pid_t pid;
		int ret;
		/*This versions fetches the status of ANY terminated child. We must specify
		 * the WNOHANG option: this tells waitpid not to block in some children are still running.
		 * We cannot perform wait in a loop because it would block if there are some children
		 * that have not yet terminated*/
		while( (pid = waitpid(-1, &ret, WNOHANG)) > 0 )
			fprintf(stdout, "(%s) --- child process %d terminated\n", prog_name, pid);
		
		signal(SIGCHLD, sign_handler);
	}
}

void service(int connfd, pid_t pid){
	int n, nwrite, fd;
	char block[BLOCKSIZE], recvline[MAXBUFL], filename[MAXBUFL], command[4];
	uint32_t size, timestamp;
	
	signal(SIGPIPE, sign_handler);
	
	fprintf(stdout, "(PID = %d) --- child process started\n", pid);
	
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
}

int main(int argc, char** argv){
	int listenfd, connfd, ret;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t addrlen;
	uint16_t servport;
	pid_t childpid;
	
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
	
	while(1){
		if(nservices >= MAXCHLD){
			/*Here we want to block until one of the children dies. So we can proceed creating a new child server process.
			 * What is to noticed is that the signal handler will not be executed here, because the father is BLOCKED so it will catch
			 * the SIGCHILD here.*/
			 fprintf(stdout, "(%s) --- maximum number of served clients reached (%d). Waiting for a child process to terminate its service\n", prog_name, MAXCHLD); fflush(stdout);
			 pid_t pid = waitpid(-1, &ret, 0); nservices--;
			 fprintf(stdout, "(%s) --- child process %d terminated\n", prog_name, pid);  fflush(stdout);
		}
		fprintf(stdout, "(%s) --- waiting for connections on %s:%s\n", prog_name, inet_ntoa(servaddr.sin_addr), argv[1]); fflush(stdout);
		bzero(&cliaddr, sizeof(cliaddr));
		addrlen = sizeof(cliaddr);
		if( (connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &addrlen)) < 0 ){
			if(connfd == EINTR)
				continue;
			else{
				fprintf(stderr, "(%s) --- accept() failed - %s\n", prog_name, strerror(errno)); continue;
			}
		}
		fprintf(stdout, "(%s) --- connection with %s:%hu estabilished\n", prog_name, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
		if( (childpid = fork()) == 0){
			//child process
			Close(listenfd);	//we can  kill the process if Close fails, no effect on the whole server
			service(connfd, getpid());	//service routine
			exit(0);
		} else {
			if(close(connfd) < 0){
				fprintf(stderr, "(%s) --- can't perform close - %s", prog_name, strerror(errno)); continue; //here we cannot kill the process
			}
			nservices++;
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












