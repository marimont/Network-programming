#include "src/myunp.h"
#include "src/mysock.h"
#include "src/errlib.h"
#include "src/mytcp.h"
#include "src/mystartup.h"
#include "src/myutils.h"

#define MAXBUFL 261
#define ERRMSG "-ERR\r\n"
#define OKMSG "+OK\r\n"
#define QUITREQ "QUIT\r\n"
#define UPDMSG "UPD\r\n"

char* prog_name;
int keepalive = 1;

int main(int argc, char** argv){
	int sockfd;
	uint32_t size, timestamp;
	char recvline[MAXBUFL], sendline[MAXBUFL], filename[MAXBUFL];

	prog_name = argv[0];
	if(argc < 4){
		err_quit("(%s) --- invalid number of parameters", prog_name);
	}

	bzero(filename, MAXBUFL);
	strcpy(filename, argv[3]);
	sockfd = Tcp_connect(argv[1], argv[2]);
	err_msg("(%s) --- connected to %s:%s\n", prog_name, argv[1], argv[2]);
	
	/*send request to the server*/
	bzero(sendline, MAXBUFL);
	sprintf(sendline, "GET%s\r\n", filename);
	if(myWriten(sockfd, sendline, strlen(sendline)) < 0){
		myClose(sockfd); return 1;
	}
	
	/*wait for response*/
	bzero(recvline, MAXBUFL);
	if(myReadline_unbuffered(sockfd, recvline, MAXBUFL) <= 0){
		myClose(sockfd); return 2;
	}
	err_msg("(%s) --- received response: '%s'\n", prog_name, recvline);
	
	/*check if ERR*/
	if(!strcmp(recvline, ERRMSG)){
		myClose(sockfd); return 3;
	}

	/*wait for timestamp*/
	if( RecvUNumber(sockfd, &timestamp) < 0){
		err_msg("(%s) --- invalid timestamp\n", prog_name); 
		myClose(sockfd);
		return 4;
	}
	err_msg("(%s) --- received timestamp: %u", prog_name, timestamp);
	
	/*wait for size*/
	if( RecvUNumber(sockfd, &size) < 0){
		err_msg("(%s) --- invalid size\n", prog_name); 
		myClose(sockfd);
		return 5;
	}
	err_msg("(%s) --- received size: %u", prog_name, size);

	/*receiving file*/
	if (myReadAndWriteToFile(sockfd, filename, size) != size ){
		err_msg("(%s) --- transmission failed\n", prog_name); 
		myClose(sockfd);
		return 6;
	}
	err_msg("(%s) --- transmission complete\n", prog_name);
	if( setFileLastMod(filename, timestamp) == FALSE){	
		err_msg("(%s) --- setFileLastMod() failed\n", prog_name); 
		myClose(sockfd); 
		return 7;
	}
	err_msg("(%s) --- file last modification set to %u\n", prog_name, timestamp);


	/*the client performs a blocking read on the socket, 
	 * waiting for updates
         */		
	while(1){
		bzero(recvline, MAXBUFL);
		if(myReadline_unbuffered(sockfd, recvline, MAXBUFL) <= 0){
			myClose(sockfd);
			break;
		}

		/*check on error msg*/
		if(!strcmp(recvline, ERRMSG)){
			myClose(sockfd); 
			break;
		}
		err_msg("(%s) --- received UPDATE msg\n", prog_name);

		/*wait for timestamp*/
		uint32_t newtimestamp;
		bzero(&newtimestamp, sizeof(newtimestamp));
		if( RecvUNumber(sockfd, &newtimestamp) < 0){
			err_msg("(%s) --- invalid timestamp\n", prog_name); 
			myClose(sockfd);
			break;
		}
		err_msg("(%s) --- received timestamp: %u", prog_name, newtimestamp);
	
		/*wait for size*/
		bzero(&size, sizeof(size));
		if( RecvUNumber(sockfd, &size) < 0){
			err_msg("(%s) --- invalid size\n", prog_name); 
			myClose(sockfd);
			break;
		}
		err_msg("(%s) --- received size: %u", prog_name, size);

		/*receiving file*/
		if (myReadAndWriteToFile(sockfd, filename, size) != size ){
			err_msg("(%s) --- transmission failed\n", prog_name); 
			myClose(sockfd);
			break;
		}
		err_msg("(%s) --- transmission complete\n", prog_name);
		if( setFileLastMod(filename, newtimestamp) == FALSE){	
			err_msg("(%s) --- setFileLastMod() failed\n", prog_name); 
			myClose(sockfd); 
			break;
		}
		err_msg("(%s) --- file last modification set to %u\n", prog_name, newtimestamp);
	}

	return 0;
}

























