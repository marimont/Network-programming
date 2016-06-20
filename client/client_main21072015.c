#include "../myunp.h"
#include "../mysock.h"
#include "../errlib.h"
#include "../types1.h"
#include "../myxdr.h"
#include "../mytcp.h"


#define MAXBUFL 261
#define msg "Insert a file name of EOF (^D) to exit: "

char *prog_name;

/*Assuming to have only relative filenames (i.e. located in the same dir), UNIX limits the filename length to 255.
*So the length of the buffer that will be sent by the client cannot exceeds 261 chars (255+6)
* */

int keepalive = 1;

int main(int argc, char** argv){
	int sockfd, nread, nwrite, fd;
	struct sockaddr_in servaddr;
	uint16_t servport;
	char *filename, *recvfile;
	int size, timestamp;
	call_msg sendmsg;
	response_msg recvmsg;
	
	prog_name = argv[0];
	
	if(argc < 4){
		err_msg("(%s) --- invalid number of arguments\n", prog_name);
	}		

	sockfd = Tcp_connect(argv[1], argv[2]);

	filename = argv[3];

	sendmsg.call_msg_u.filename = filename;
	sendmsg.ctype = GET;

	if( myStdIOWriteXdr(sockfd, (xdr_function)&xdr_call_msg, (void*)&sendmsg) == FALSE){
		myClose(sockfd); return 3;
	}
	err_msg("(%s) --- sent msg 'GET %s'\n", prog_name, filename);
	if (myStdIOReadXdr(sockfd, (xdr_function)&xdr_response_msg, (void*)&recvmsg) == FALSE){
		myClose(sockfd); return 3;
	}
	if(recvmsg == ERR){
		err_msg("(%s) --- received msg 'ERR'\n", prog_name);
		myClose(sockfd);
		return 3;
	}
	if( RecvUNumber(sockfd, &size) != sizeof(uint32_t)){
		myClose(sockfd);
		return 3;
	}
	err_msg("(%s) --- received size: %u\n", prog_name, size);

	if(myReadAndWriteToFile(sockfd, filename, size) < 1){
		myClose(sockfd);
		return 3;
	}

	err_msg("(%s) --- file received and stored", prog_name);

	sendmsg.ctype = QUIT;

	if( myStdIOWriteXdr(sockfd, (xdr_function)&xdr_call_msg, (void*)&sendmsg) == FALSE){
		myClose(sockfd); return 3;
	}
	err_msg("(%s) --- sent msg 'QUIT'\n", prog_name);
	
	return 3;
}
