/* blink.c
 *
 * Raspberry Pi GPIO example using sysfs interface.
 * Guillermo A. Amaral B. <g@maral.me>
 *
 * This file blinks GPIO 4 (P1-07) while reading GPIO 24 (P1_18).
 */

/* 

  HID USB interface probe

  Scan for HID USB devices, recognize supported devices, and add device to list of active devices.
  Scanning is non intrusive. 

  By: Simon Rigét @ Paragi 2021
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
#include <stddef.h>
#include <wchar.h>
#include <stdbool.h>
#include <dirent.h> 

/* Unix */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <poll.h>
#include <grp.h>

/* Linux */
#include <hidapi/hidapi.h>
#include <glib.h>

/* Application */
#include "toolbox.h"
#include "common.h"

#include "sysfs.h"

#ifdef TEST_GPIO

#define IN  0
#define OUT 1

#define LOW  0
#define HIGH 1

#define PIN  24 /* P1-18 */
#define POUT 4  /* P1-07 */

static int
GPIOExport(int pin)
{
#define BUFFER_MAX 3
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open export for writing!\n");
		return(-1);
	}

	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}

static int
GPIOUnexport(int pin)
{
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open unexport for writing!\n");
		return(-1);
	}

	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}

static int
GPIODirection(int pin, int dir)
{
	static const char s_directions_str[]  = "in\0out";

#define DIRECTION_MAX 35
	char path[DIRECTION_MAX];
	int fd;

	snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio direction for writing!\n");
		return(-1);
	}

	if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
		fprintf(stderr, "Failed to set direction!\n");
		return(-1);
	}

	close(fd);
	return(0);
}

static int
GPIORead(int pin)
{
#define VALUE_MAX 30
	char path[VALUE_MAX];
	char value_str[3];
	int fd;

	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for reading!\n");
		return(-1);
	}

	if (-1 == read(fd, value_str, 3)) {
		fprintf(stderr, "Failed to read value!\n");
		return(-1);
	}

	close(fd);

	return(atoi(value_str));
}

static int
GPIOWrite(int pin, int value)
{
	static const char s_values_str[] = "01";

	char path[VALUE_MAX];
	int fd;

	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for writing!\n");
		return(-1);
	}

	if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
		fprintf(stderr, "Failed to write value!\n");
		return(-1);
	}

	close(fd);
	return(0);
}
#endif

void myfilerecursive(char *path);

#ifndef _WCHAR_T_DEFINED
// VSCode has a problem with using include paths....
typedef unsigned short wchar_t;
#endif


/* 
  probe for HID USB devices that match relay drivers.
  When matched, add aan entry to the device list.
*/  
int probe_sysfs(int si_index, struct _device_identifier id, GSList **device_list){
  //const struct _supported_device * supported_device;
  //struct dirent *dp;
  struct stat stat_buffer;
  //DIR *dir;
  char * test_path[] = {"/sys/class/gpio", "/sys/class/thermal", "/sys/class/lirc", NULL};
  assert(supported_interface[si_index].name);

  // Check that /sys/class exists
  if (!access("/sys/class", F_OK)) {
    
    if(id.device_id){
      test_path[0] = id.device_id;
      test_path[1] = NULL;
    }

    for(int i = 0; test_path[i]; i++){
      if (!stat(test_path[i], &stat_buffer) && S_ISDIR(stat_buffer.st_mode)) {
        struct _device_list *entry;
        struct group *grp;        
        
        // Create a new entry in active device list
        entry = malloc(sizeof(struct _device_list)); 
        entry->name = sdsnew("sysfs");
        entry->id = id.device_id ? 
          sdsnew(id.device_id) : 
          sdscatprintf(sdsempty(),".../%s",sdsnew( g_path_get_basename(strndup(test_path[i],strlen(test_path[i])+1))));
        entry->path = sdsnew(id.device_id ? :"n/a");
        grp = getgrgid(stat_buffer.st_gid);
        entry->group = sdsnew(grp->gr_name);
        entry->action = action_sysfs;
        *device_list = g_slist_append(*device_list, entry);
        if ( info ) 
          printf(" -- Recognized as %s\n",entry->name);
      }
    }
  } else if ( info ) 
    printf(" -- Not recognized\n");

  return SUCCESS;
} 

void myfilerecursive(char *basePath)
{
    char path[1000];
    struct dirent *dp;
    DIR *dir = opendir("/sys/class");

   
    if (!dir)
        return;

    while ((dp = readdir(dir)) != NULL)
    {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
        {
            printf("%s\n", dp->d_name);
            strcpy(path, basePath);
            strcat(path, "/");
            strcat(path, dp->d_name);

            //myfilerecursive(path);
        }
    }

    closedir(dir);
}


int action_sysfs(struct _device_list *device, sds attribute, sds action, sds *reply){
  int return_code = SUCCESS;
  struct stat stat_buffer;

  printf("SysFs on: %s  Action: %s\n",attribute, action);

  if (stat(device->id, &stat_buffer) ){
    perror(device->id);
    return FAILURE;
  }

  if( !S_ISDIR(stat_buffer.st_mode)) {
    fprintf(stderr, "%s is a a valid SysFS path to a device\n", device->id);
    return FAILURE;
  }

  if ( attribute ) {
    sds file_path = sdscatprintf(sdsempty(), "%s/%s",device->id, attribute); 
    do {
      if ( access(file_path, F_OK) ) {
        fprintf(stderr,"%s is not an accessable attribute of %s\n", attribute, device->id);
        return_code =  FAILURE;
        break;
      }

      // Write top device attribute
      if ( action ) {
        if ( access(file_path,W_OK) ){
          fprintf(stderr,"This user has no write access to %s\n", file_path);
          return_code =  FAILURE;
          break;
        }

        printf("Writing to %s : %s\n", attribute, action);

      // read from device attribute
      } else { 
        if ( access(file_path,R_OK) ){
          fprintf(stderr,"This user has no read access to %s\n", file_path);
          return_code =  FAILURE;
          break;
        }
        printf("Reading from %s\n", attribute);

      }

    } while( 0 );
    sdsfree(file_path);
  }

  return return_code;
}

