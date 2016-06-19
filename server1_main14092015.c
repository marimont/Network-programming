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

char *prog_name;

int keepalive = 1;

static void connection_closed(int signo){
	fprintf(stdout, "(%s) - connection closed by the client\n", prog_name);
	keepalive = 0; 
	signal(SIGPIPE, connection_closed);
}


int main(int argc, char** argv){
  struct sockaddr_storage cliaddr;
  socklen_t cliaddrlen;
  int listenfd, connfd, n, fd;
  char recvline[MAXBUFL], sendline[MAXBUFL], command[5], filename[MAXBUFL], block[BLOCKSIZE];
  bool_t redirect = FALSE;
  uint32_t timestamp, size;
	
   prog_name = argv[0];

  if(argc < 2){
    err_quit("(%s) --- invalid number of parameters", prog_name);
  } else if(argc == 4){
    redirect = TRUE;
  }
  
  signal(SIGPIPE, connection_closed);
  listenfd = Tcp_listen(NULL, argv[1], NULL);

  while(1){
    keepalive = 1;
    bzero(&cliaddr, sizeof(cliaddr));
    cliaddrlen = sizeof(cliaddr);
    fprintf(stdout, "(%s) --- waiting for connections\n", prog_name);
    if((connfd = myAccept(listenfd, (SA*)&cliaddr, &cliaddrlen)) < 0 )
      continue;
    fprintf(stdout, "(%s) --- connected to client %s. Waiting for commands\n", prog_name, Sock_ntop((SA*)&cliaddr, cliaddrlen));
    if( (n = myReadline_unbuffered(connfd, recvline, MAXBUFL)) < 0){
      myClose(connfd);
      continue;
    }else if(n == 0){
      fprintf(stderr, "(%s) --- connection closed by the client\n", prog_name); myClose(connfd); continue;
    }
    
    fprintf(stdout, "(%s) --- received command '%s'\n", prog_name, recvline);
     if(!strcmp(recvline, QUITREQ)){
       fprintf(stdout, "(%s) --- closing connection to the client\n", prog_name);
       myClose(connfd);
       continue;
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
	  sprintf(sendline, "-RED%s %s\r\n", argv[2], argv[3]);
	  if( myWriten(connfd, sendline, strlen(sendline)) == 0)
	    fprintf(stdout, "(%s) --- sent message '%s'\n", prog_name, sendline);
	  
	} else {
	  /*send err msg*/
	  if( myWriten(connfd, ERRMSG, 6) == 0)
	    fprintf(stdout, "(%s) --- sent message '%s'\n", prog_name, ERRMSG);
	}
	myClose(connfd);
	continue;
     }
 
     if( myWriten(connfd, OKMSG, 5) < 0){
	 myClose(connfd); continue;
     }
	    fprintf(stdout, "(%s) --- sent message '%s'\n", prog_name, OKMSG);
     size = (uint32_t) buf.st_size;
     uint32_t  size_32 = htonl(size);
     if( myWriten(connfd, &size_32, sizeof(size)) < 0){
       myClose(connfd); continue;
     }
     fprintf(stdout, "(%s) --- size sent: '%u'\n", prog_name, size);
    
     fprintf(stdout, "(%s) --- trasmission of file '%s' starting...\n", prog_name, filename);
     if(myTcpReadFromFileAndWriteChunks(connfd, filename, NULL, size, 0) < 0){
	 fprintf(stdout, "(%s) --- trasmission failed\n", prog_name);
	}
	fprintf(stdout, "(%s) --- trasmission complete\n", prog_name);
    
     myClose(connfd);
  }
  myClose(listenfd);
  return 0;
}
 
