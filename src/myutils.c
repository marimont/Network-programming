#include "myunp.h"
#include <utime.h>
#include "myutils.h"

extern char *prog_name;

long int getFileSize(const char* filename){
	struct stat buf;
	int n = stat(filename, &buf);
	if(n < 0)
		return -1;
	return buf.st_size;
}

long int getFileLastMod(const char* filename){
	struct stat buf;
	int n = stat(filename, &buf);
	if(n < 0)
		return -1;
	return  buf.st_mtim.tv_sec;
}

long int getFileLastAccess(const char* filename){
	struct stat buf;
	int n = stat(filename, &buf);
	if(n < 0)
		return -1;
	return  buf.st_atim.tv_sec;
}

bool_t setFileLastMod(const char* filename, long int mtime){
	struct utimbuf buf;
	buf.modtime = mtime;
	int n = utime(filename, &buf);
	if(n != 0)
		return FALSE;
	return  TRUE;
}

bool_t setFileLastAccess(const char* filename, long int atime){
	struct utimbuf buf;
	buf.actime = atime;
	int n = utime(filename, &buf);
	if(n != 0)
		return FALSE;
	return  TRUE;
}










