#include "src/myunp.h"
#include "src/mysock.h"
#include "src/errlib.h"
#include "src/types1.h"
#include "src/myxdr.h"
#include "src/mytcp.h"


#define LISTENQ 15
	/*Assuming to have only relative filenames (i.e. located in the same dir), UNIX limits the filename length to 255.
	*So the length of the buffer that will be sent by the client cannot exceeds 261 chars (255+6)
	* */
#define MAXBUFL 261
#define BLOCKSIZE 1048576


char *prog_name;

int keepalive;

int min(int x, int y);

static void connection_closed(int signo){
	fprintf(stdout, "(%s) - connection closed by the client\n", prog_name);
	keepalive = 0; 
}

long int fileSize(FILE *fp) {
    fseek(fp, 0L, SEEK_END);
    long int file_size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    return file_size;
}


int main(int argc, char** argv){
	int listenfd, connfd, n, nwrite, fd;
	struct sockaddr_in servaddr, cliaddr;
	uint16_t servport;
	char *filename, block[BLOCKSIZE];
	socklen_t addrlen;
	int size, timestamp;
	response_msg sendmsg;
	call_msg recvmsg;
	
	prog_name = argv[0];
	
	if(argc < 2){
		fprintf(stderr, "(%s) --- Error: invalid number of parameters. You should provide a port number\n", prog_name);
		return 1;
	}
	
	listenfd = Tcp_listen("0.0.0.0", argv[1], NULL);
	signal(SIGPIPE, connection_closed);
	while(1){
		fprintf(stdout, "(%s) --- waiting for connections on 0.0.0.0:%s\n", prog_name, argv[1]);
		addrlen = sizeof(cliaddr);
		bzero(&cliaddr, sizeof(cliaddr));
		connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &addrlen);
		if(connfd < 0){
			fprintf(stderr, "(%s) --- accept() failed - %s\n", prog_name, strerror(errno)); continue;
		}
		fprintf(stdout, "(%s) --- connection estabilished with client at %s:%u\n", prog_name, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
		
		keepalive = 1;
		/*Wait for client requests*/
		while(keepalive){
			fprintf(stdout, "(%s) --- Waiting for commands...\n", prog_name); fflush(stdout);	
			bzero(&recvmsg, sizeof(recvmsg));
			if(!myStdIOReadXdr(connfd, (xdr_function)&xdr_call_msg, (void*)&recvmsg)){
				fprintf(stderr, "(%s) --- can't read client request\n", prog_name); break;
			}

			if(recvmsg.ctype == QUIT){
			/* -QUIT msg received*/
				fprintf(stderr, "(%s) --- received QUIT command\n", prog_name);
				break;;	//break the single-client loop -> wait for new connections
			}
			
			bzero(&filename, sizeof(filename));
			fprintf(stderr, "(%s) --- received request: 'GET %s'\n", prog_name, recvmsg.call_msg_u.filename);
			filename = recvmsg.call_msg_u.filename;
		
			bzero(&sendmsg, sizeof(sendmsg));

			
			FILE *fp = fopen(filename, "r");
			if(fp == NULL){
				sendmsg = ERR;
				myStdIOWriteXdr(connfd, (xdr_function)&xdr_response_msg, (void*)&sendmsg);
				fprintf(stdout, "(%s) --- ERR message sent\n", prog_name);
				continue;
			}

			sendmsg = OK;
			if(!myStdIOWriteXdr(connfd, (xdr_function)&xdr_response_msg, (void*)&sendmsg)){
				fprintf(stderr, "(%s) --- can't send response message\n", prog_name); break;
			}
			fprintf(stdout, "(%s) --- OK message sent\n", prog_name);
			size = fileSize(fp);
			fclose(fp);
			if (SendUNumber(connfd, size) != sizeof(uint32_t)){
				myClose(connfd); break;
			}
			fprintf(stdout, "(%s) --- size %u sent\n", prog_name, size);

			fprintf(stdout, "(%s) --- trasmission of file '%s' starting...\n", prog_name, filename);
			if(myTcpReadFromFileAndWriteChunks(connfd, filename, size, BLOCKSIZE) != size){
				fprintf(stdout, "(%s) --- trasmission failed\n", prog_name);
			} else 
				fprintf(stdout, "(%s) --- trasmission complete\n", prog_name);
		}
		myClose(connfd);
	}

	Close(listenfd);
	
	return 0;
}


