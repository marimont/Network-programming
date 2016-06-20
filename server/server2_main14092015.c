#include "src/myunp.h"
#include "src/mysock.h"
#include "src/errlib.h"
#include "src/mystartup.h"
#include "src/mytcp.h"

#define LISTENQ 15
#define BLOCKSIZE 1048576
#define MAXBUFL 261
#define ERRMSG "-ERR\r\n"
#define OKMSG "+OK\r\n"
#define QUITREQ "QUIT\r\n"

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

int service(int connfd, char* servaddr, char* servport, pid_t pid, bool_t redirect){
    int  n;
    uint32_t size;
    char recvline[MAXBUFL], sendline[MAXBUFL], filename[MAXBUFL];

    sprintf(prog_name, "%s_%d", prog_name, pid);	 

    bzero(recvline, sizeof(recvline));
    if( (n = myReadline_unbuffered(connfd, recvline, MAXBUFL)) < 0){
      myClose(connfd);
      return 5; 
    }else if(n == 0){
      fprintf(stderr, "(%s) --- connection closed by the client\n", prog_name); myClose(connfd); return 1;
    }
    
    fprintf(stdout, "(%s) --- received command '%s'\n", prog_name, recvline);
     if(!strcmp(recvline, QUITREQ)){
       fprintf(stdout, "(%s) --- closing connection to the client\n", prog_name);
       myClose(connfd);
       return 6;
      }

     /*check on request format correctness*/
    
     bzero(&filename, sizeof(filename)); 
     strcpy(filename, recvline + 3);
     filename[strlen(filename) - strlen("\r\n")] = '\0';
     struct stat buf;
     n = stat(filename, &buf);
     if(n < 0){
	fprintf(stdout, "(%s) - not able to stat: %s\n", prog_name, strerror(errno));
	if(redirect){
	  /*send redirect message*/
	  sprintf(sendline, "-RED%s %s\r\n", servaddr, servport);
	  if( myWriten(connfd, sendline, strlen(sendline)) == 0)
	    fprintf(stdout, "(%s) --- sent message '%s'\n", prog_name, sendline);
	  
	} else {
	  /*send err msg*/
	  if( myWriten(connfd, ERRMSG, 6) == 0)
	    fprintf(stdout, "(%s) --- sent message '%s'\n", prog_name, ERRMSG);
	}
	myClose(connfd);
	return 2;
     }
 
     if( myWriten(connfd, OKMSG, 5) < 0){
	 myClose(connfd); return 3;
     }
	    fprintf(stdout, "(%s) --- sent message '%s'\n", prog_name, OKMSG);
     size = (uint32_t) buf.st_size;
    
     uint32_t  size_32 = htonl(size);
     if( myWriten(connfd, &size_32, sizeof(size)) < 0){
       myClose(connfd); return 4;
     }
     fprintf(stdout, "(%s) --- size sent: '%u'\n", prog_name, size);
    
     fprintf(stdout, "(%s) --- trasmission of file '%s' starting...\n", prog_name, filename);
     if(myTcpReadFromFileAndWriteChunks(connfd, filename, NULL, size, 0) < 0){
	 fprintf(stdout, "(%s) --- trasmission failed\n", prog_name);
	}
	fprintf(stdout, "(%s) --- trasmission complete\n", prog_name);

    return 0;
}


int main(int argc, char** argv){
  struct sockaddr_storage cliaddr;
  socklen_t cliaddrlen;
  int listenfd, connfd;
  bool_t redirect = FALSE; 

  int servicen = 0;
	
   prog_name = argv[0];

  if(argc < 2){
    err_quit("(%s) --- invalid number of parameters", prog_name);
  } else if(argc >= 4){
    redirect = TRUE;
  }
  
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
	service(connfd, argv[2], argv[3], getpid(), redirect);
	exit(0);
    }
    fprintf(stdout, "(%s) --- launched process %d\n", prog_name, childpid[servicen]);
    myClose(connfd);

  }
  myClose(listenfd);
  return 0;
}
 
