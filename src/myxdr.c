#include "myunp.h"
#include "errlib.h"
#include "mysock.h"
#include "myxdr.h"

extern char* prog_name;

bool_t myStdIOReadXdr(int sockfd, xdr_function xdr_func, void *data){
		
	FILE *fp;
	XDR xdrs;
	
	if( (fp = fdopen(dup(sockfd), "r")) == NULL){
		fprintf(stderr, "(%s) --- can't perform fdopen - %s\n", prog_name, strerror(errno)); return FALSE;
	}
	xdrstdio_create(&xdrs, fp, XDR_DECODE);
	if(!xdr_func(&xdrs, data)){
		fprintf(stderr, "(%s) --- can't read from xdr stream - %s\n", prog_name, strerror(errno)); 
		xdr_destroy(&xdrs);
		fclose(fp);
		return FALSE;
	}
	
	xdr_destroy(&xdrs);
	fclose(fp);
	return TRUE;
}


bool_t myStdIOWriteXdr(int sockfd, xdr_function xdr_func, void *data){
	FILE *fp;
	XDR xdrs;
	
	if( (fp = fdopen(dup(sockfd), "w")) == NULL){
		fprintf(stderr, "(%s) --- can't perform fdopen - %s\n", prog_name, strerror(errno)); return FALSE;
	}
	
	if(!xdr_func(&xdrs, data)){
		fprintf(stderr, "(%s) --- can't read from xdr stream - %s\n", prog_name, strerror(errno)); 
		xdr_destroy(&xdrs);
		fclose(fp);
		return FALSE;
	}
	
	xdr_destroy(&xdrs);
	fclose(fp);
	return TRUE;

}

/*mem stream functions calls sequence (input)
 * n = read(sockfd, buf, maxlen)
 * xdrs = myMemReadXDR_setup(buf, n);
 * myMemReadXDR(&xdrs, (xdr_function*)&xdr_func, (void*)&data); //data <- deserialized data
 * myMemXDRDestroy(&xdrs);
 * */

XDR myMemReadXDR_setup(char* buf, u_int len){
	XDR xdrs;
	xdrmem_create(&xdrs, buf, len, XDR_DECODE);
	return xdrs;
}
bool_t myMemReadXDR(struct XDR* xdrs, xdr_function xdr_func, void *data){
	if(!xdr_func(xdrs, data)){
		fprintf(stderr, "(%s) --- can't read from buffer - %s\n", prog_name, strerror(errno)); 
		return FALSE;
	}
	return TRUE;
}

XDR myMemWriteXDR_setup(char* buf, u_int len){
	XDR xdrs;
	xdrmem_create(&xdrs, buf, len, XDR_ENCODE);
	return xdrs;
}

int myMemWriteXDR(struct XDR* xdrs, xdr_function xdr_func, void *data){
	if(!xdr_func(xdrs, data)){
		fprintf(stderr, "(%s) --- can't write on buffer - %s\n", prog_name, strerror(errno)); 
		return -1;
	}
	return xdr_getpos(xdrs);
}

void myMemXDRDestroy(struct XDR *xdrs){
	xdr_destroy(xdrs);
}

/* mem stream functions calls sequence (output)
 * xdrs = myMemWriteXDR_setup(buf, n);
 * pos = myMemWriteXDR(&xdrs, (xdr_function*)&xdr_func, (void*)&data); //data <- serialized data
 * write(sockfd, buf, pos);
 * myMemXDRDestroy(&xdrs);
 * */


