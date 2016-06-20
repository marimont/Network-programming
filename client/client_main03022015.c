/*
 *  TEMPLATE
 */
#include "src/myunp.h"
#include "src/mysock.h"
#include "src/errlib.h"
#include "src/mytcp.h"
#include "src/mystartup.h"
#include "src/utils.h"

#define BLOCKSIZE 1048576
#define MAXBUFL 255

char *prog_name;
int keepalive = 1;

int main (int argc, char *argv[])
{	int sockfd, fd, N;
	char sendblock[BLOCKSIZE], filename[MAXBUFL];
	uint32_t size, hash;

	prog_name = argv[0];
	if(argc < 5){
		err_quit("(%s) --- invalid number of parameters\n", prog_name);
	}

	N = atoi(argv[3]);
	strcpy(filename, argv[4]);

	size = getFileSize(filename);
	if(size < 0){
		err_quit("(%s) --- file not found\n", prog_name); return 1;
	} else if (size < BLOCKSIZE){
		err_quit("(%s) --- the requested file doesn't contain enough bytes\n", prog_name); return 1;
	}

	sockfd = Tcp_connect(argv[1], argv[2]);
	err_msg("(%s) --- connected to %s:%s\n", prog_name, argv[1], argv[2]);
	fd = open(filename, O_RDONLY);
	bzero(sendblock, BLOCKSIZE);
	if(read(fd, sendblock, BLOCKSIZE) != BLOCKSIZE){
		err_msg("(%s) --- read from file failed\n", prog_name);
		myClose(sockfd); return -1;
	}
	close(fd);
	while(N){
		if(myWriten(sockfd, sendblock, BLOCKSIZE) < 0){
			myClose(sockfd); break;
		}
	N--;
	}

	if(RecvUNumber(sockfd, &hash) != sizeof(uint32_t)){
		err_msg("(%s) --- hash code reception failed\n", prog_name);
		myClose(sockfd); return -1;
	}
	fprintf(stdout, "%u\n", hash);
	myClose(sockfd);

	return 0;
}





