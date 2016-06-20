#include "myunp.h"
#include "mysock.h"
#include "errlib.h"
#include "mytcp.h"

#define DEFAULT_CHUNK_SIZE 1048576

extern int keepalive;

static bool_t myTcpReadChunksAndWriteToFileCallback(void *chunk, int chunkSize, void *param) ;

int SendUNumber(SOCKET sock, uint32_t num) {
    uint32_t net_num = htonl(num);
    int res;
    res = send(sock, &net_num, sizeof(uint32_t), 0);

    return res;
}

int RecvUNumber(SOCKET sock, uint32_t* num) {
    uint32_t net_num;
    int res;
    res = read(sock, &net_num, sizeof(uint32_t));

    *num = ntohl(net_num);
    return res;
}

int myReadAndWriteToFile(SOCKET sockfd, const char* filename, uint32_t size){
	int nread, nwrite, fd;
	char *recvfile = (char*) malloc(size*sizeof(char));
	nread = readn(sockfd, recvfile, size);
	if(nread != size){
		err_msg("(%s) --- error reading from socket\n"); return -1;
	}
	fd = creat(filename, 0777);
	nwrite = writen(fd, recvfile, nread);
	if(nwrite != nread){
		err_msg("(%s) --- error writing file\n"); return -1;
	}	
	free(recvfile);
	close(fd);
	return nwrite;
}


int myTcpReadChunks(int sockfd, int byteCount, int *readByteCount, myTcpReadChunksCallback callback, void *callbackParam) {
  int leftBytes, numberOfReadBytes, chunkSize;
  void *buffer;
  

  buffer = (char*)malloc(sizeof(char) * DEFAULT_CHUNK_SIZE);
  
  leftBytes = byteCount;
  if (readByteCount != NULL)
    *readByteCount = 0;
  
  while (leftBytes > 0) {

    if (leftBytes < DEFAULT_CHUNK_SIZE)
      chunkSize = leftBytes;
    else
      chunkSize = DEFAULT_CHUNK_SIZE;
    
    numberOfReadBytes = readn(sockfd, buffer, chunkSize);
    
    if (numberOfReadBytes <= 0) {
      free(buffer);
      return -1;
    }
    
    if (readByteCount != NULL)
      *readByteCount = *readByteCount + numberOfReadBytes;

    if (callback != NULL) {
      if (callback(buffer, numberOfReadBytes, callbackParam) == FALSE) {
	free(buffer);
	return -1;
      }
    }
    
    leftBytes -= numberOfReadBytes;
  }
  
  free(buffer);
  return 0;
}

int myTcpReadAndWriteToFile(int sockfd, const char *filePath, int fileSize){
	char *buffer;
	FILE *fp;

	buffer = (char*)malloc(fileSize*sizeof(char));
	if(read(sockfd, buffer, fileSize) <= 0)
	 return -1;
	
	fp = fopen(filePath, "w");
	if(fp == NULL)
		return -1;
	
	fputs(buffer, fp);
	fclose(fp);
	return 0;
}


int myTcpReadChunksAndWriteToFile(int sockfd, const char *filePath, int fileSize, int *readByteCount) {
  FILE *fd;
  int reply = 0;
  
  fd = fopen(filePath, "w");
  if (fd == NULL){
    err_msg("fopen"); return -1;
 }
  
  reply = myTcpReadChunks(sockfd, fileSize, readByteCount, &myTcpReadChunksAndWriteToFileCallback, (void*)fd);
  
  if (fclose(fd) == EOF){
   err_msg("fclose");
   return -1;
  }

  return reply;
}

int min(int x, int y){
	int min = x;
	if(y < x)
		min = y;
	return min;
}

int myTcpReadFromFileAndWriteChunks(int sockfd, const char *filePath, int fileSize, int chunkSize) {
 int fd;
  int numberOfWrittenBytes = 0;
  int toBeSent = fileSize, n, myChunkSize;
  char *buffer;

  if(chunkSize != 0){
	myChunkSize = chunkSize;  
   } else
	myChunkSize = DEFAULT_CHUNK_SIZE;

   buffer = (char*) malloc(myChunkSize*sizeof(char));
  
  fd = open(filePath, O_RDONLY);
  if (fd < 0){
   err_msg("fopen"); return -1;
  }

	while(toBeSent > 0 && keepalive){
		 bzero(buffer, myChunkSize);
		int chunk;
		if(toBeSent < myChunkSize)
			chunk = toBeSent;
		else 
			chunk =  myChunkSize;
		n = read(fd, buffer, chunk);
		if(myWriten(sockfd, buffer,n) < 0)
			return -1;
		toBeSent -= n;
		numberOfWrittenBytes += n;
	} 
	
  free(buffer);
  if (myClose(fd) < 0){
    err_msg("close");
    return -1;
  }
  
  return numberOfWrittenBytes;
}

static bool_t myTcpReadChunksAndWriteToFileCallback(void *chunk, int chunkSize, void *param) {
  if (fwrite(chunk, 1, chunkSize, (FILE*)param) != chunkSize)
    return FALSE;
  return TRUE;
}

