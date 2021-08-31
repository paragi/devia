/*
  Simon's toolbox
  (c) 2021 Simon Rigét @ paragi.dk

  Free to copy, with reference to author
*/

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
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

/* Linux */
#include "sds.h"
#include "toolbox.h"


#define SUCCESS 0
#define FAILURE -1

// Convert a BLOB to readable hex
sds sdsbytes2hex(void * byte_str, int bytes, int block_size) {
  sds str = sdsempty();
  
  block_size = abs(block_size/2);
  if( block_size < 1 ) block_size = 1;
  
  for (int i = 0, blkp = 0; i< bytes; i++) {
    str = sdscatprintf(str,"%.02X", ((unsigned char *)byte_str)[i]); 
    if (++blkp >= block_size ) {
      str = sdscatprintf(str," ");
      blkp = 0;
    } 
  }
  return str;  
}  

// Convert int to binary string
sds sdsint2bin(long int value ,int len) {
  char buffer[65];
  sds str;

  if(len+1 > sizeof(buffer)) len = sizeof(buffer)-1;
  buffer[len] = 0;
  for (int i = len-1; i >= 0; i--) 
    buffer[i] = value & (1<<(len-i-1)) ? '1' : '0';

  return str = sdsnew(buffer);  
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

// Create a ls -l like file permission string for debug purposes 
// Modified version of code by askovpen
sds file_permissions_string(char * path){
  struct group *grp;   
  struct passwd *pw;
  sds permission_str;
  struct stat stat_buffer;
  static const char *rwx[] = {"---", "--x", "-w-", "-wx","r--", "r-x", "rw-", "rwx"};
  static char bits[11];

  if (stat(path, &stat_buffer) )
    return sdsnew("File does not exists or is inaccessible") ;

  // Set file type 
  if (S_ISREG(stat_buffer.st_mode))         bits[0] = '-';
  else if (S_ISDIR(stat_buffer.st_mode))    bits[0] = 'd';
  else if (S_ISBLK(stat_buffer.st_mode))    bits[0] = 'b';
  else if (S_ISCHR(stat_buffer.st_mode))    bits[0] = 'c';
  #ifdef S_ISFIFO  
    else if (S_ISFIFO(stat_buffer.st_mode)) bits[0] = 'p';
  #endif  
  #ifdef S_ISLNK
    else if (S_ISLNK(stat_buffer.st_mode))  bits[0] = 'l';
  #endif 
  #ifdef S_ISSOCK
    else if (S_ISSOCK(stat_buffer.st_mode)) bits[0] = 's';
  #endif 
  /* Solaris 2.6, etc. */
  #ifdef S_ISDOOR
    else if (S_ISDOOR(stat_buffer.st_mode)) bits[0] = 'D';
  #endif  
  // Unknown type -- possibly a regular file? 
  else                                      bits[0] = '?';

  // Set rwx for owner, group  and anyone
  strcpy(&bits[1], rwx[(stat_buffer.st_mode >> 6)& 7]);
  strcpy(&bits[4], rwx[(stat_buffer.st_mode >> 3)& 7]);
  strcpy(&bits[7], rwx[(stat_buffer.st_mode & 7)]);
  if (stat_buffer.st_mode & S_ISUID)
    bits[3] = (stat_buffer.st_mode & S_IXUSR) ? 's' : 'S';
  if (stat_buffer.st_mode & S_ISGID)
    bits[6] = (stat_buffer.st_mode & S_IXGRP) ? 's' : 'l';
  if (stat_buffer.st_mode & S_ISVTX)
    bits[9] = (stat_buffer.st_mode & S_IXOTH) ? 't' : 'T';
  bits[10] = '\0';

  pw = getpwuid(stat_buffer.st_uid);
  grp = getgrgid(stat_buffer.st_gid); 
  permission_str = sdscatprintf(sdsempty(),"%s %s:%s",bits,pw->pw_name,grp->gr_name);

  return permission_str ;
}

/*
  Return a human readable string, that explains the requirements to
  do the requested operation.
  operrations are as defined im unistd.h :
    R_OK 4	Test for read permission. 
    W_OK 2	Test for write permission.  
    X_OK 1	Test for execute permission.  
    F_OK 0  Test taht file exists

  if the current user has permissions, return an empty string.
*/
sds file_permission_needed(char * path, int access_type){
  struct group *grp;   
  struct passwd *pw;
  struct stat stat_buffer;
  static char current_username[100];

  access_type &= 7;

  if (stat(path, &stat_buffer) )
    return sdsnew("does not exists or is inaccessible");

  if ( !access(path, access_type) ) 
    return sdsempty();

  pw = getpwuid(stat_buffer.st_uid);
  grp = getgrgid(stat_buffer.st_gid);
  getlogin_r(current_username, sizeof(current_username));

  // Test group
  if ( (stat_buffer.st_mode >> 3) & access_type ) 
    return sdscatprintf(
      sdsempty(),
      "must be a member for group '%s' (usermod -aG %s %s)",
      grp->gr_name,
      grp->gr_name,
      current_username
    );
  
  // Test user
  if ( (stat_buffer.st_mode >> 6) & access_type ) 
    return sdscatprintf(
      sdsempty(),
      "login as '%s' ",
      pw->pw_name
    );

  return sdsnew("not accessible");
}
