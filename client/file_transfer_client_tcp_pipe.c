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

	/*Assuming to have only relative filenames (i.e. located in the same dir), UNIX limits the filename length to 255.
	*So the length of the buffer that will be sent by the client cannot exceeds 261 chars (255+6)
	* */
#define MAXBUFL 261
#define ERRMSG "-ERR\r\n"
#define OKMSG "+OK\r\n"
#define QUITREQ "QUIT\r\n"
#define msg "Insert a file name of EOF (^D) to exit: "

char *prog_name;
pid_t childpid;

static void sig_int(int signo) {

	err_msg ("(%s) info - sig_int() called", prog_name);
	kill(childpid, SIGTERM);
	wait(NULL);

	if (errno != ECHILD)
		err_quit("(%s) error: wait() error");

	exit(0);
}


static void signal_handler(int signo){
	if(signo == SIGCHLD){
		waitpid(childpid, NULL, 0);
		/*I kill the client if the transer process crashed, but I release memory, by catching its retvalue*/
		/*no need to reinstall*/
		/*it releases all resources, so no big pain*/
		exit(0);
	}
}

void printMenu(){
	fprintf(stdout, "(%s) --- Select one option among the following ones:\n\tGET to start a file transfer\n\tQ to terminate a filetransfer and close the connection\n\tA to forcefully terminate the connection\n", prog_name);
}

void service(int sockfd, int fdin, pid_t pid){
	int nread, nwrite, fd;
	char *recvfile, filename[MAXBUFL], answ[6], sendline[MAXBUFL];
	uint32_t size, timestamp;
	
	prog_name = "tranfer process";
	
	while((nread = read(fdin, filename, sizeof(filename))) > 0){
		bzero(&sendline, sizeof(sendline));
		sprintf(sendline, "GET %s\r\n", filename); 
		nwrite = writen(sockfd, sendline, strlen(sendline));
		if(nwrite != strlen(sendline)){
			fprintf(stderr, "(%s) --- can't send GET command\n", prog_name); continue;
		}
		
		bzero(&answ, sizeof(answ));
		if( (nread = readline_unbuffered(sockfd, answ, 8)) < 0){
			fprintf(stderr, "(%s) --- can't perform readline() - %s\n", prog_name, strerror(errno)); continue;
		} else if(nread == 0){
			fprintf(stderr, "(%s) --- connection closed by the server\n", prog_name); break;
		}
		if(!strcmp(answ, "-ERR\r\n")){
			fprintf(stdout, "(%s) --- received answer '%s'\n", prog_name, answ);
			bzero(&filename, sizeof(filename));
			continue;
		}
		
		if( (nread = readn(sockfd, &size, sizeof(size))) < 0){
			fprintf(stderr, "(%s) --- can't perform read() - %s\n", prog_name, strerror(errno)); continue;
		} else if(nread == 0){
			fprintf(stderr, "(%s) --- connection closed by the server\n", prog_name); break;
		}
		
		uint32_t size_32 =ntohl(size);
		
		if( (nread = readn(sockfd, &timestamp, sizeof(timestamp))) < 0){
			fprintf(stderr, "(%s) --- can't perform read() - %s\n", prog_name, strerror(errno)); continue;
		} else if(nread == 0){
			fprintf(stderr, "(%s) --- connection closed by the server\n", prog_name); break;
		}
		
		//uint32_t timestamp_32 =ntohl(timestamp);		
		fd = open(filename, O_WRONLY | O_CREAT, 0777);
		
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
	
	fprintf(stdout, "Leaving\n");
}

int main(int argc, char** argv){
	int sockfd, nwrite, pi[2], sigact_res;
	struct sockaddr_in servaddr;
	uint16_t servport;
	char filename[MAXBUFL], sendline[MAXBUFL];
	struct sigaction action;

	
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
	if(pipe(pi) < 0){
		fprintf(stderr, "(%s) --- error creating a pipe - %s\n", prog_name, strerror(errno)); 
		close(sockfd);
		return 2;
	}
	
	signal(SIGCHLD, signal_handler);
	if((childpid = fork()) == 0){
		/*childproc*/
		close(pi[1]);			/*Write end is unused*/
		service(sockfd, pi[0], getpid());
		close(pi[0]);
		exit(0);
	}
	
	/*father proc*/
	/* This is to capture CTRL-C and terminate children 
	 * Otherwise all children remain alive in the OS!!!*/	
	memset(&action, 0, sizeof (action));
	action.sa_handler = sig_int;
	sigact_res = sigaction(SIGINT, &action, NULL);
	if (sigact_res == -1)
		err_quit("(%s) sigaction() failed", prog_name);

	
	close(pi[0]);	/*read end is unused*/
	
	char choise[5];
	while(1){
		printMenu();
		bzero(&choise, sizeof(choise));
		mygetline(choise, 5, ">");
		if(!strcmp(choise, "GET")){
			bzero(&filename, sizeof(filename));
			mygetline(filename, MAXBUFL, "filename >");
			/*pass the filename to the child process throught the pipe*/
			nwrite = write(pi[1], filename, strlen(filename));
			if(nwrite != strlen(filename)){
				fprintf(stderr, "(%s) --- cannot write on the pipe - %s\n", prog_name, strerror(errno)); 
				kill(childpid, SIGTERM);
				waitpid(childpid, NULL, 0);
				fprintf(stdout, "(%s) --- forcing execution termination\n", prog_name);
				break;
			}
			fprintf(stdout, "(%s) --- '%s' enqueued for tranfering\n", prog_name, filename);
		} else if(!strcmp(choise, "Q")){
			close(pi[1]);             /* Child will see EOF */
			waitpid(childpid, NULL, 0);
			bzero(&sendline, sizeof(sendline));
			sprintf(sendline, "QUIT\r\n"); 
			nwrite = writen(sockfd, sendline, strlen(sendline));
			if(nwrite != strlen(sendline)){
				fprintf(stderr, "(%s) --- can't send QUIT command\n", prog_name); continue;
			}
			fprintf(stdout, "(%s) --- QUIT command sent\n", prog_name);
			break;
		} else if(!strcmp(choise, "A")){
			kill(childpid, SIGTERM);
			waitpid(childpid, NULL, 0);
			fprintf(stdout, "(%s) --- forcing execution termination\n", prog_name);
			break;
		} else{
			fprintf(stderr, "(%s) --- invalid option\n", prog_name); continue;
		}
		
	}
	if(pi[1] > 0)
		close(pi[1]);
	Close(sockfd);
	return 0;
	
	
}





















