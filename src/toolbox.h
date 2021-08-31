#ifndef _sds_extra
#define _sds_extra

#include "sds.h"

// SDS extensions
sds sdsint2bin(long int value, int len );
sds sdsbytes2hex(void * byte_str, int bytes, int block_size);

// char * extensions
char *strtounical(char * source);
char *strtoupper(char * source);
char *strtolower(char * source); 
char *int2bin(int value ,int len, char *buffer, int buf_size);

sds file_permission_needed(char * path, int access_type);
sds file_permissions_string(char * path);

#endif