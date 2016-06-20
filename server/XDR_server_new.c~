/*This server sends the file within the body of the message as a block.
 * This could be a bad idea in case of big files -> it would be better to send
 *separately tag, size, timestamp and then the file*/
 

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


char *prog_name;

int keepalive;

int min(int x, int y);

static void connection_closed(int signo){
	fprintf(stdout, "(%s) - connection closed by the client\n", prog_name);
	keepalive = 0; 
}

int main(int argc, char** argv){
	int listenfd, connfd, n, nwrite, fd;
	struct sockaddr_in servaddr, cliaddr;
	uint16_t servport;
	char *filename;
	socklen_t addrlen;
	int size, timestamp;
	message sendmsg, recvmsg;
	file sendfile;
	
	prog_name = argv[0];
	
	if(argc < 2){
		fprintf(stderr, "(%s) --- Error: invalid number of parameters. You should provide a port number\n", prog_name);
		return 1;
	}
	
	listenfd = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	fprintf(stdout, "(%s) --- socket created\n", prog_name);
	
	servport = atoi(argv[1]);
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(servport);
	
	Bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
	
	Listen(listenfd, LISTENQ);	
	signal(SIGPIPE, connection_closed);
	while(1){
		fprintf(stdout, "(%s) --- waiting for connections on %s:%s\n", prog_name, inet_ntoa(servaddr.sin_addr), argv[1]);
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
			if(!myStdIOReadXdr(connfd, (xdr_function)&xdr_message, (void*)&recvmsg)){
				fprintf(stderr, "(%s) --- can't read client request\n", prog_name); break;
			}

			if(recvmsg.tag == QUIT){
			/* -QUIT msg received*/
				fprintf(stderr, "(%s) --- received QUIT command\n", prog_name);
				break;	//break the single-client loop -> wait for new connections
			}
			
			bzero(&filename, sizeof(filename));
			fprintf(stderr, "(%s) --- received request: 'GET %s'\n", prog_name, recvmsg.message_u.filename);
			filename = recvmsg.message_u.filename;
		
			bzero(&sendmsg, sizeof(sendmsg));

			struct stat buf;
			n = stat(filename, &buf);
			if(n < 0){
				fprintf(stdout, "(%s) - not able to stat: %s\n", prog_name, strerror(errno));
				sendmsg.tag = ERR;
				myStdIOWriteXdr(connfd, (xdr_function)&xdr_message, (void*)&sendmsg);
				fprintf(stdout, "(%s) --- ERR message sent\n", prog_name);
				continue;
			}
			size = (int) buf.st_size;
			fprintf(stdout, "(%s) --- size '%d'\n", prog_name, size);
			timestamp = (int) buf.st_mtim.tv_sec;
			fprintf(stdout, "(%s) --- timestamp '%d'\n", prog_name, timestamp);
			
			sendmsg.tag = OK;
			
			bzero(&sendfile, sizeof(sendfile));
			sendfile.contents.contents_len = size;
			sendfile.last_mod_time = timestamp;
			
			fd = open(filename, O_RDONLY);
			sendfile.contents.contents_val = (char*) malloc(size*sizeof(char));
			n = read(fd, sendfile.contents.contents_val, size);
			if(n != size){
				fprintf(stdout, "(%s) --- file scanning failed - %s\n", prog_name, strerror(errno));
				continue;
			}
			close(fd);
			
			sendmsg.message_u.fdata = sendfile;
			if(!myStdIOWriteXdr(connfd, (xdr_function)&xdr_message, (void*)&sendmsg)){
				fprintf(stdout, "(%s) --- trasmission failed\n", prog_name);
			}
			fprintf(stdout, "(%s) --- trasmission complete\n", prog_name);

		}
		if (close(connfd) != 0){
			 fprintf(stderr, "(%s) --- connected socket close() failed - %s\n", prog_name, strerror(errno));
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


