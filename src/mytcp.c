#include "myunp.h"
#include "mysock.h"
#include "errlib.h"
#include "mytcp.h"

#define DEFAULT_CHUNK_SIZE 1048576*4

extern int keepalive;

static bool_t myTcpReadChunksAndWriteToFileCallback(void *chunk, int chunkSize, void *param) ;


int myTcpReadChunks(int sockfd, int byteCount, int *readByteCount, myTcpReadChunksCallback callback, void *callbackParam) {
  int leftBytes, numberOfReadBytes, chunkSize;
  void *buffer;
  

  buffer = (void*)malloc(sizeof(void) * DEFAULT_CHUNK_SIZE);
  
  leftBytes = byteCount;
  if (readByteCount != NULL)
    *readByteCount = 0;
  
  while (leftBytes > 0) {

    if (leftBytes < DEFAULT_CHUNK_SIZE)
      chunkSize = leftBytes;
    else
      chunkSize = DEFAULT_CHUNK_SIZE;
    
    numberOfReadBytes = readn(sockfd, buffer, chunkSize);
    
    if (numberOfReadBytes <= 0 ||  numberOfReadBytes != chunkSize) {
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


int myTcpReadChunksAndWriteToFile(int sockfd, const char *filePath, int fileSize, int *readByteCount) {
  FILE *fd;
  int reply = 0, fdes;

  fdes = creat(filePath, 0777);
  fd = fdopen(fdes, "w");
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

int myTcpReadFromFileAndWrite(int sockfd, const char *filePath, int *writtenByteCount, int size) {
 int fd;
  int numberOfWrittenBytes = 0;
  int toBeSent = size, n;
  void *buffer = malloc(DEFAULT_CHUNK_SIZE*sizeof(void));
  
  fd = open(filePath, O_RDONLY);
  if (fd < 0){
   err_msg("fopen"); return -1;
  }
	/*n = read(fd, buffer,size);*/
	/*if(myWriten(sockfd, buffer, strlen(buffer)) < 0)
			return -1;*/
	while(toBeSent > 0 && keepalive){
		 bzero(buffer, DEFAULT_CHUNK_SIZE);
		n = read(fd, buffer, min(toBeSent, DEFAULT_CHUNK_SIZE));
		if(myWriten(sockfd, buffer, strlen(buffer)) < 0)
			return -1;
		toBeSent -= n;
		numberOfWrittenBytes += n;
	} 
	
  
  if (myClose(fd) < 1){
    err_msg("close");
    return -1;
  }
  
  if (writtenByteCount != NULL)
    *writtenByteCount = numberOfWrittenBytes;
  
  return 0;
}

static bool_t myTcpReadChunksAndWriteToFileCallback(void *chunk, int chunkSize, void *param) {
  if (fwrite(chunk, 1, chunkSize, (FILE*)param) != chunkSize)
    return FALSE;
  return TRUE;
}

