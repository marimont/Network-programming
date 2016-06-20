#include "../myunp.h"
#include "../mysock.h"
#include "../errlib.h"
#include "../mystartup.h"
#include "../mytcp.h"

#define LISTENQ 15
#define BLOCKSIZE 1048576
#define MAXBUFL 261
#define ERRMSG "-ERR\r\n"
#define OKMSG "+OK\r\n"
#define QUITREQ "QUIT\r\n"
#define UPDMSG "UPD\r\n"

#define MAXCHILDS 3

char *prog_name;

int keepalive = 1;

pid_t childpid[MAXCHILDS];

static void connection_closed(int signo){

	if(signo == SIGPIPE){
		fprintf(stdout, "(%s) - connection closed by the client\n", prog_name);
		keepalive = 0; 
		signal(SIGPIPE, connection_closed);	
	} else if(signo == SIGCHLD){
		pid_t pid;
		while( (pid = waitpid(-1,NULL, WNOHANG)) > 0 )
			fprintf(stdout, "(%s) --- child process %d terminated\n", prog_name, pid);
		
		signal(SIGCHLD, connection_closed);
	}
}

int service(int connfd, int N, pid_t pid){
    int  n, s;
    uint32_t size, timestamp;
    struct timeval tval;
    fd_set rset;	
    char recvline[MAXBUFL], sendline[MAXBUFL], filename[MAXBUFL];

    sprintf(prog_name, "%s_%d", prog_name, pid);	 

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


int main(int argc, char** argv){
  struct sockaddr_in cliaddr;
  socklen_t cliaddrlen;
  int listenfd, connfd;
  int N;

  int servicen = 0;
	
   prog_name = argv[0];

  if(argc < 3){
    err_quit("(%s) --- invalid number of parameters", prog_name);
  } 
  
  N = atoi(argv[2]);
  signal(SIGPIPE, connection_closed);
  signal(SIGCHLD, connection_closed);
	
  listenfd = Tcp_listen(NULL, argv[1], NULL);

  while(1){
    keepalive = 1;
    bzero(&cliaddr, sizeof(cliaddr));
    if(servicen >= MAXCHILDS){
	fprintf(stdout, "(%s) --- no more connections can be accepted\n", prog_name); 
	wait(NULL);
	servicen--;
    }
    cliaddrlen = sizeof(cliaddr);
    fprintf(stdout, "(%s) --- waiting for connections\n", prog_name);
    if((connfd = myAccept(listenfd, (SA*)&cliaddr, &cliaddrlen)) < 0 )
      continue;
    servicen++;
    		 
    if((childpid[servicen] = fork()) == 0){
		/*child*/
	service(connfd, N, getpid());
	exit(0);
    }
    fprintf(stdout, "(%s) --- launched process %d\n", prog_name, childpid[servicen]);
    myClose(connfd);

  }
  myClose(listenfd);
  return 0;
}
 
