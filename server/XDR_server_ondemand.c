#include "myunp.h"
#include "mysock.h"
#include "errlib.h"
#include "myxdr.h"
#include "types.h"

#define LISTENQ 15

	/*Assuming to have only relative filenames (i.e. located in the same dir), UNIX limits the filename length to 255.
	*So the length of the buffer that will be sent by the client cannot exceeds 261 chars (255+6)
	* */
#define MAXBUFL 261
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
	int n, fd;
	char *filename;
	int size, timestamp;
	message sendmsg, recvmsg;
	file sendfile;
	
	signal(SIGPIPE, sign_handler);
	
	fprintf(stdout, "(PID = %d) --- child process started\n", pid);
	
	keepalive = 1;
	/*Wait for client requests*/
	while(keepalive){
		fprintf(stdout, "(%d) --- Waiting for commands...\n", pid); fflush(stdout);	
			bzero(&recvmsg, sizeof(recvmsg));
			if(!myStdIOReadXdr(connfd, (xdr_function)&xdr_message, (void*)&recvmsg)){
				fprintf(stderr, "(%d) --- can't read client request\n", pid); break;
			}

			if(recvmsg.tag == QUIT){
			/* -QUIT msg received*/
				fprintf(stderr, "(%d) --- received QUIT command\n", pid);
				break;	//break the single-client loop -> wait for new connections
			}
			
			bzero(&filename, sizeof(filename));
			fprintf(stderr, "(%d) --- received request: 'GET %s'\n", pid, recvmsg.message_u.filename);
			filename = recvmsg.message_u.filename;
		
			bzero(&sendmsg, sizeof(sendmsg));

			struct stat buf;
			n = stat(filename, &buf);
			if(n < 0){
				fprintf(stdout, "(%d) - not able to stat: %s\n", pid, strerror(errno));
				sendmsg.tag = ERR;
				myStdIOWriteXdr(connfd, (xdr_function)&xdr_message, (void*)&sendmsg);
				fprintf(stdout, "(%d) --- ERR message sent\n", pid);
				continue;
			}
			size = (int) buf.st_size;
			fprintf(stdout, "(%d) --- size '%d'\n", pid, size);
			timestamp = (int) buf.st_mtim.tv_sec;
			fprintf(stdout, "(%d) --- timestamp '%d'\n", pid, timestamp);
			
			sendmsg.tag = OK;
			
			bzero(&sendfile, sizeof(sendfile));
			sendfile.contents.contents_len = size;
			sendfile.last_mod_time = timestamp;
			
			fd = open(filename, O_RDONLY);
			sendfile.contents.contents_val = (char*) malloc(size*sizeof(char));
			n = read(fd, sendfile.contents.contents_val, size);
			if(n != size){
				fprintf(stdout, "(%d) --- file scanning failed - %s\n", pid, strerror(errno));
				continue;
			}
			close(fd);
			
			sendmsg.message_u.fdata = sendfile;
			if(!myStdIOWriteXdr(connfd, (xdr_function)&xdr_message, (void*)&sendmsg)){
				fprintf(stdout, "(%d) --- trasmission failed\n", pid);
				bzero(&sendmsg, sizeof(sendmsg));
				sendmsg.tag = ERR;
				myStdIOWriteXdr(connfd, (xdr_function)&xdr_message, (void*)&sendmsg);
				fprintf(stdout, "(%d) --- ERR message sent\n", pid);
				continue;
			}
			fprintf(stdout, "(%d) --- trasmission complete\n", pid);

		
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











