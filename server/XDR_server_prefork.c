#include "myunp.h"
#include "mysock.h"
#include "errlib.h"
#include "types.h"
#include "myxdr.h"

#define LISTENQ 15
	/*Assuming to have only relative filenames (i.e. located in the same dir), UNIX limits the filename length to 255.
	*So the length of the buffer that will be sent by the client cannot exceeds 261 chars (255+6)
	* */
#define MAXBUFL 261
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
	int n, fd, connfd;
	char *filename;
	struct sockaddr_in cliaddr;
	socklen_t addrlen;
	int size, timestamp;
	message sendmsg, recvmsg;
	file sendfile;
	
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












