#ifndef _sds_extra
#define _sds_extra

#include <glib.h>

#include "sds.h"

// SDS extensions
sds sdsint2bin(long int value ,unsigned int len);
sds sdsbytes2hex(void * byte_str, int bytes, int block_size);

// char * extensions
char *strtounical(char * source);
char *strtoupper(char * source);
char *strtolower(char * source); 
char *int2bin(int value ,int len, char *buffer, int buf_size);

sds file_permission_needed(char * path, int access_type);
sds file_permissions_string(char * path);
GList *finddir(char *basepath, char *searchdir);
void finddir_free(GList *list);
int file_put( char *file_name, void *data, int length );
void * file_get(char * file_name, int *length);

#endif