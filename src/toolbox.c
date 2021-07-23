
/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <malloc.h>
#include <argp.h>

/* Unix */


/* Linux */


#include "sds.h"
#include "toolbox.h"


#define SUCCESS 0
#define FAILURE -1

// Convert int to binary sds string
sds sdsint2bin(long int value, int len ) {
  sds binstr = sdsnew("0000000000000000000000000000000000000000000000000000000000000000");
  int str_end = strlen(binstr) -1;
  for (int i = str_end; i >= 0; i--){
    binstr[i] = value & (1l<<(str_end-i)) ? '1' : '0';
  }
  sdsrange(binstr,-len,-1);
  return binstr;
}

// Convert int to binary string
char *int2bin(int value ,int len, char *buffer, int buf_size) {
  if(len+1 > buf_size) len = buf_size-1;
  buffer[len] = 0;
  for (int i = len-1; i >= 0; i--) 
    buffer[i] = value & (1<<(len-i-1)) ? '1' : '0';
  return buffer;
}

char *strtolower(char * source) {
  for (int i = strlen(source)-1; i >= 0; --i) 
    source[i] = tolower(source[i]);
  return source;  
}

char *strtoupper(char * source) {
  for (int i = strlen(source)-1; i >= 0; --i) 
    source[i] = toupper(source[i]);
  return source;  
}

char *strtounical(char * source) {
  for (int i = strlen(source)-1; i >= 0; --i)
    if( i == 0 )
      source[i] = toupper(source[i]);
    else if ( source[i-1] == ' ' )
      source[i] = toupper(source[i]);
    else  
      source[i] = tolower(source[i]);
  return source;  
}

