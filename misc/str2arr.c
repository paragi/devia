
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

/* Unix */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <poll.h>

/* Linux */
#include <linux/hidraw.h>
#include <linux/version.h>
#include <linux/input.h>
#include <libudev.h>
#include <hidapi/hidapi.h>


#include <argp.h>

#define SUCCESS 0
#define FAILURE -1

typedef struct string {
  char *data;
  size_t len;
  void *set;
  void (*del)(void*);
  void * concat;
  struct string * substr;
} string_t;

typedef struct{
  char ** at;
  size_t length;
  char *data;
  size_t data_length;
  void (*del)(void*);
} array_t;


//typedef char **array_t;

typedef struct {
  char ** at_key;
  array_t key;
  char ** at_data;
  array_t data;
} vector_t;


// Get unique identifier of physical port
        // /sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.6/2-1.6:1.0/0003:0416:5020.0004/hidraw/hidraw3
        //             |----------------- Unique id --------------------|                   |------|  

/*
string to array

char **array = str2arr(char * string, char * delimiter)

Split a string by any charcter in delimiter, and return an array of pointers to each string fragment.
The string is modified, in htat all occurences of delimiter characters is replaced with a null terminator. 

Return: An array of pointer is always returned. The last element points to NULL.

NB: the array it self must be freed by the caller.

*/

array_t str2arr(char *string, char *delimiter) {
  array_t arr =  {NULL,0,"\0",0,NULL};
  int i, i_len = 0;
  char *strp, *p2;

  if ( string ) {
    arr.data_length = strlen(string);
    arr.data = strdup(string);
    assert(arr.data);

    // Replace delimiter chars with null terminator.
    strp = arr.data;
    printf("Working on: %s (%s) \n",strp, delimiter);
    for(i_len = 0; (p2 = strtok_r(strp, delimiter, &strp)); i_len++)
      printf("%d-%s\n",i_len,p2);
  }

  puts("#1");
  printf("Allocating %ld bytes of memory for array index\n", sizeof(char *) * i_len);
  arr.at = malloc(sizeof(char *) * i_len);   
  assert(arr.at);
  // Terminate array index
  arr.at[i_len] = NULL;
    
  // Populate index array 
  strp = arr.data;
  for(i = 0; i<i_len; i++){
    printf("Filling  %s\n",strp);
    arr.at[i] = strp;
    strp += strlen(strp)+1;
  }

  puts("#3");

  printf("Array = {\n");
  for(int i = 0; arr.at[i]; i++)
    printf("  %s\n",arr.at[i]);
  printf("}\n");

  return arr;
}

//#define TEST

#ifdef TEST

int main(){
  array_t arr;
  char *str;
  char *test_case[] = {
    "Dette er en test",
    "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.6/2-1.6:1.0/0003:0416:5020.0004/hidraw/hidraw3",
    "",
    NULL
  };
   
  arr = str2arr(test_case[0]," ");

  printf("Array = {\n");
  for(int i = 0; arr.at[i]; i++)
    printf("%d  %s\n",i,arr.at[i]);
  printf("}\n");
    
  return SUCCESS;
}
#endif