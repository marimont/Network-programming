#include "src/myunp.h"
#include "src/mysock.h"
#include "src/mystartup.h"
#include "src/mytcp.h"
#include "src/myutils.h"
#include "src/errlib.h"

#define MAXBUFL 25
#define OKMSG "+OK\r\n"
#define ERRMSG "-ERR\r\n"

char *prog_name;
int keepalive = 1;
int
main(int argc, char* argv[])
{
	int sockfd;
	char filename[MAXBUFL], sendline[MAXBUFL], recvline[MAXBUFL];
	uint32_t size;
	
	prog_name = argv[0];
	if(argc < 4)
		err_quit("(%s) --- invalid number of parameters\n", prog_name);

	strcpy(filename, argv[3]);

	sockfd = Tcp_connect(argv[1], argv[2]);
	
	sprintf(sendline, "PUT%s\r\n", filename);
	if(writen(sockfd, sendline, strlen(sendline)) != strlen(sendline)){
		err_msg("(%s) --- error sending command\n", prog_name); 
		myClose(sockfd);
		return 1;
	}
	fprintf(stdout, "(%s) --- sent command %s\n", prog_name, sendline);

	/*wait for answer*/
	if(myReadline_unbuffered(sockfd, recvline, MAXBUFL) < 0){
		err_msg("(%s) --- readline failed\n", prog_name); 
		myClose(sockfd);
		return 1;
	}	
	fprintf(stdout, "(%s) --- received answer: %s\n", prog_name, recvline);
	if(!strcmp(recvline, ERRMSG)){
		myClose(sockfd); return 1;
	}

	size = (uint32_t) getFileSize(filename);
	if(myTcpReadFromFileAndWriteChunks(sockfd, filename, size, 0) != size){
		err_msg("(%s) --- transmission failed", prog_name);
	}else 
		fprintf(stdout, "(%s) --- transmission complete", prog_name);

	myClose(sockfd);

	return 0;
}
