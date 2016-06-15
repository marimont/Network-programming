/*Functions defined to manage, almost, automatically xdr streams, 
 * both stdio and mem streams*/

#ifndef	__myxdr_h
#define	__myxdr_h

typedef bool_t (*xdr_function)(struct XDR *, const void *);

bool_t myStdIOReadXdr(int sockfd, xdr_function xdr_func, void *data);
bool_t myStdIOWriteXdr(int sockfd, xdr_function xdr_func, void *data);
XDR myMemReadXDR_setup(char* buf, u_int len);
bool_t myMemReadXDR(struct XDR* xdrs, xdr_function xdr_func, void *data);
XDR myMemWriteXDR_setup(char* buf, u_int len);
int myMemWriteXDR(struct XDR* xdrs, xdr_function xdr_func, void *data);
void myMemXDRDestroy(struct XDR *xdrs);

#endif
