#ifndef	__MYTCP_H
#define	__MYTCP_H

typedef bool_t (*myTcpReadChunksCallback)(void *chunk, int chunkSize, void *param);
typedef bool_t (*myTcpWriteChunksCallback)(void *chunk, int *chunkSize, void *param);

int myTcpReadChunks(int sockfd, int byteCount, int *readByteCount, myTcpReadChunksCallback callback, void *callbackParam);
int myTcpReadChunksAndWriteToFile(int sockfd, const char *filePath, int fileSize, int *readByteCount);
int myTcpReadFromFileAndWrite(int sockfd, const char *filePath, int *writtenByteCount, int size);

#endif
