#ifndef _MYUTILS_H
#define	_MYUTILS_H

long int getFileSize(const char* filename);
long int getFileLastMod(const char* filename);
long int getFileLastAccess(const char* filename);

bool_t setFileLastMod(const char* filename, long int mtime);
bool_t setFileLastAccess(const char* filename, long int atime);
#endif
