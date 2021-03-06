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

static void connection_closed(int signo){
	fprintf(stdout, "(%s) - connection closed by the client\n", prog_name);
	keepalive = 0; 
	signal(SIGPIPE, connection_closed);
}

int main(int argc, char** argv){
	struct sockaddr_in cliaddr;
	socklen_t addrlen;
	char sendline[MAXBUFL], recvline[MAXBUFL], filename[MAXBUFL];
	uint32_t size, timestamp;
	int N, listenfd, connfd, s;
	struct timeval tval;
	fd_set rset;

	prog_name = argv[0];
	
	if(argc < 3){
		err_msg("(%s) --- invalid number of parameters\n"); return 1;
	}
	
	signal(SIGPIPE, connection_closed);
	listenfd = Tcp_listen("0.0.0.0", argv[1], NULL);
	while(1){
		bzero(&cliaddr, sizeof(cliaddr));
		addrlen = sizeof(cliaddr);
		fprintf(stdout, "(%s) --- waiting for connections\n", prog_name);
		if( (connfd = myAccept(listenfd, (SA*) &cliaddr, &addrlen)) < 0){
			continue;		
		}
		fprintf(stdout, "(%s) --- connected to client %s. Waiting for commands\n", prog_name, Sock_ntop((SA*)&cliaddr, addrlen));
		keepalive = 1;
		while(keepalive){
			/*manage first request*/
			if(myReadline_unbuffered(connfd, recvline, MAXBUFL) < 0){
				myClose(connfd); break;
			}
			err_msg("(%s) --- received command: '%s'\n", prog_name, recvline);
			if(!strcmp(recvline, QUITREQ)){
				myClose(connfd); break;
			}
			bzero(filename, MAXBUFL);
			strcpy(filename, recvline+3);
			filename[strlen(filename) - strlen("\r\n")] = '\0';
			size = (uint32_t) getFileSize(filename);
			timestamp = (uint32_t) getFileLastMod(filename);
			if(size < 0 || timestamp < 0){
				/*error*/
				sprintf(sendline, "%s", ERRMSG);
				myWriten(connfd, sendline, strlen(sendline));
				err_msg("(%s) --- send  ERR msg\n", prog_name);
				continue;
			}
			
			sprintf(sendline, "%s", OKMSG);
			if(myWriten(connfd, sendline, strlen(sendline)) < 0){
				myClose(connfd); break;
			}
			err_msg("(%s) --- sent OK msg\n", prog_name);
			
			SendUNumber(connfd, timestamp);
			err_msg("(%s) --- sent timestamp %d\n", prog_name, timestamp);

			SendUNumber(connfd, size);
			err_msg("(%s) --- sent size %d\n", prog_name, size);

			if(myTcpReadFromFileAndWriteChunks(connfd, filename, size, 0) < 0){
				err_msg("(%s) --- transmission failed\n", prog_name);
			} else{
				err_msg("(%s) --- transmission succeded\n", prog_name);
			}
			
			N = atoi(argv[2]);
			while(1){
				tval.tv_sec = N;
				tval.tv_usec = 0;
				FD_ZERO(&rset);
				FD_SET(connfd, &rset);
				if((s = select(FD_SETSIZE, &rset, NULL, NULL, &tval)) < 0){
					err_msg("(%s) --- select() failed\n", prog_name); myClose(connfd); break;
				} 
				if(s != 0){
					break;	/*new request received*/			
				}
				/*timeout expired: check update*/
				uint32_t newtimestamp = (uint32_t)getFileLastMod(filename);
				if(newtimestamp < 0){
					err_msg("(%s) --- error reading for update\n", prog_name);
					myClose(connfd); keepalive = 0; break;					
				}
				if(newtimestamp > timestamp){
				 	size = (uint32_t) getFileSize(filename);
					if(size < 0){
						err_msg("(%s) --- error reading for update\n", prog_name);
						myClose(connfd); keepalive = 0; break;					
					}
					sprintf(sendline, "%s", UPDMSG);
					if(myWriten(connfd, sendline, strlen(sendline)) < 0){
						myClose(connfd); break;
					}
					err_msg("(%s) --- sent UPD msg\n", prog_name);
					SendUNumber(connfd, newtimestamp);
					err_msg("(%s) --- sent new timestamp %d\n", prog_name, newtimestamp);

					SendUNumber(connfd, size);	
					err_msg("(%s) --- sent new size %d\n", prog_name, size);
					
					if(myTcpReadFromFileAndWriteChunks(connfd, filename, size, 0) < 0){
						err_msg("(%s) --- transmission failed\n", prog_name);
					} else{
						err_msg("(%s) --- transmission succeded\n", prog_name);
					}
					timestamp = newtimestamp;
					
				}
			
			}
		}
	}
	myClose(listenfd);

	return 0;
}
