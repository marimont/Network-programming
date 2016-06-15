#include "myunp.h"
#include "mysock.h"
#include "errlib.h"
#include "types.h"
#include "myxdr.h"

#define MAXBUFL 261
#define msg "Insert a file name of EOF (^D) to exit: "

char *prog_name;

/*Assuming to have only relative filenames (i.e. located in the same dir), UNIX limits the filename length to 255.
*So the length of the buffer that will be sent by the client cannot exceeds 261 chars (255+6)
* */

int main(int argc, char** argv){
	int sockfd, nread, nwrite, fd;
	struct sockaddr_in servaddr;
	uint16_t servport;
	char filename[MAXBUFL];
	int size, timestamp;
	message sendmsg, recvmsg;
	
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
	bzero(&filename, sizeof(filename));
	while(1){
		if (mygetline(filename, MAXBUFL, msg) == EOF){
			bzero(&sendmsg, sizeof(sendmsg));
			sendmsg.tag = QUIT;
			myStdIOWriteXdr(sockfd, (xdr_function)&xdr_message, (void*)&sendmsg);
			break;
		}
		bzero(&sendmsg, sizeof(sendmsg));
		sendmsg.tag = GET;
		sendmsg.message_u.filename = filename;
		if(!myStdIOWriteXdr(sockfd, (xdr_function)&xdr_message, (void*)&sendmsg)){
			fprintf(stderr, "(%s) --- can't send GET command\n", prog_name); continue;
		}
		fprintf(stdout, "(%s) --- sent command 'GET %s'\n", prog_name, sendmsg.message_u.filename);

		bzero(&recvmsg, sizeof(recvmsg));
		if(!myStdIOReadXdr(sockfd, (xdr_function)&xdr_message, (void*)&recvmsg)){
			fprintf(stderr, "(%s) --- can't retrieve response\n", prog_name); continue;
		}

		if(recvmsg.tag == ERR){
			fprintf(stdout, "(%s) --- received ERR msg\n", prog_name);
			bzero(&filename, sizeof(filename));
			continue;
		}

		file recvfile = recvmsg.message_u.fdata;
		size = recvfile.contents.contents_len;
		timestamp = recvfile.last_mod_time;
		fprintf(stdout, "(%s) --- received size: %u\n", prog_name, size);
		fprintf(stdout, "(%s) --- received timestamp: %u\n", prog_name, timestamp);
		
		fd = open(filename, O_WRONLY | O_CREAT);
		nwrite = write(fd, recvfile.contents.contents_val, size);
		if(nwrite != size ){
			fprintf(stderr, "(%s) - incomplete file\n", prog_name); 
		}else{
			fprintf(stdout, "(%s) --- '%s' has been received and stored\n", prog_name, filename);
		}	
		
		close(fd);
		bzero(&filename, sizeof(filename));
	}
	
	Close(sockfd);
	return 0;
	
	
}
