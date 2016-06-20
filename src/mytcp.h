#ifndef	__MYTCP_H
#define	__MYTCP_H

typedef bool_t (*myTcpReadChunksCallback)(void *chunk, int chunkSize, void *param);
typedef int SOCKET;

int RecvUNumber(SOCKET sock, uint32_t* num);
int SendUNumber(SOCKET sock, uint32_t num);

int myReadAndWriteToFile(SOCKET sockfd, const char* filename, uint32_t size);
int myTcpReadChunksAndWriteToFile(int sockfd, const char *filePath, int fileSize, int *readByteCount);
int myTcpReadFromFileAndWriteChunks(int sockfd, const char *filePath, int fileSize, int chunkSize);
int myTcpReadAndWriteToFile(int sockfd, const char *filePath, int fileSize);

#endif
