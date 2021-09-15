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
#include <pwd.h>
#include <grp.h>

/* Linux */
#include <hidapi/hidapi.h>
#include <glib.h>

/* Application */
#include "toolbox.h"
#include "common.h"

#include "sysfs.h"

#ifndef _WCHAR_T_DEFINED
// VSCode has a problem with using include paths....
typedef unsigned short wchar_t;
#endif

/* 
  probe for HID USB devices that match relay drivers.
  When matched, add aan entry to the device list.
*/  
int probe_sysfs(int si_index, struct _device_identifier id, GList **device_list){
  struct _device_list *entry;
  struct group *grp;        
  struct stat stat_buffer;
  sds path = NULL;
  GList * path_list = NULL, *iterator = NULL;;
  char * buffer = NULL;
  struct dirent *dp;
  DIR *dir;


  assert(supported_interface[si_index].name);

  // Check that /sys/class exists
  if ( access("/sys/devices", F_OK) ) {
    if ( info ) printf("No sysFs\n");
    return FAILURE;
  }

  if(id.device_id){
    // Full path
    if (strchr( id.device_id,'/' ) ) {
      if ( !strncmp(id.device_id,(char * )"/sys/",5) ){
        buffer = realpath(id.device_id,NULL);
        path = sdsnew(buffer);
        free(buffer);
      // Path relative to /sys/devices/
      } else {
        sds path1 = sdscatprintf(sdsempty(),"/sys/devices/%s",id.device_id);
        buffer = realpath(path1,NULL);
        sdsfree(path1);
        path = sdsnew(buffer);
        free(buffer);

      }

      path_list = g_list_append(path_list, path);
    // find dir  
    } else {

      path_list = finddir((char *)"/sys/devices",id.device_id);
    }

  // Nothing to do  
  }  else {
    return SUCCESS;
  }
 
  if ( ! path_list ) {
    if ( info ) puts("No sysfs path found");
      return FAILURE;
  }

  for (iterator = path_list; iterator; iterator = iterator->next) {
  
    // Check that path is within /sys/
    if ( iterator->data && sdslen( (sds)iterator->data )  ) {      
      if ( strncmp((char * )iterator->data,(char * )"/sys/",5) ){
        fprintf(stderr,"%s is out of bounds path.\n",(char *)iterator->data);
        return FAILURE;
      }
    } 
    assert( iterator->data );

    if (stat((char *)iterator->data, &stat_buffer) && S_ISDIR(stat_buffer.st_mode)) {
      perror("");
      return FAILURE;
    }

    if( !S_ISDIR(stat_buffer.st_mode) ) {
      fprintf(stderr,"%s is not a directory.\n", (char *)iterator->data);
      continue;
    }

    // List files
    dir = opendir((char *)iterator->data);
    if (!dir) {
      perror("Failed");
      return FAILURE;
    }

    while ((dp = readdir(dir)) != NULL) {
      if ( dp->d_type != DT_REG )  
        continue;

      // Create a new entry in active device list
      entry = (struct _device_list *) malloc(sizeof(struct _device_list)); 
      memset(entry, 0, sizeof(struct _device_list));
      *device_list = g_list_append(*device_list, entry); 

      entry->name = sdsnew(dp->d_name);
      entry->id = sdscatprintf(sdsempty(),"sysfs#%s",(char *)iterator->data);
      entry->path = sdscatprintf(sdsempty(),"%s/%s",(char *)iterator->data, dp->d_name);
      entry->group = file_permissions_string(entry->path);
      entry->action = action_sysfs;


      if ( info ) 
        printf(" -- Recognized as %s\n",entry->name);
    }
    closedir(dir);
  }
  return SUCCESS;
} 

int action_sysfs(struct _device_list *device, sds attribute, sds action, sds *reply){
  struct stat stat_buffer;

  if ( info )
    printf("SysFs on: %s  Action: %s\n",attribute, action);

  // Check that device id is a valid direstory in SysFs
  if (stat(device->id, &stat_buffer) ){
    perror(device->id);
    return FAILURE;
  }

  if( !S_ISDIR(stat_buffer.st_mode)) {
    fprintf(stderr, "%s is not a valid path to a SysFS device\n", device->id);
    return FAILURE;
  }

  if ( attribute ) {
    int fd;
    int length;
    char input[1025];
    
    sds file_path = sdscatprintf(sdsempty(), "%s/%s",device->id, attribute); 
    
    // Check permissions
    sds permission_needed = file_permission_needed(file_path, action ? W_OK : R_OK );
    if ( sdslen(permission_needed) ){
      puts(permission_needed);
      sdsfree(permission_needed);
      return FAILURE;
    }
    sdsfree(permission_needed);
    
    // open device attribute
    if( (fd = open(file_path, action ? O_WRONLY : O_RDONLY)) < 0) {
      perror("Failed to open sysfs file");
      *reply = sdsnew("Off-line");
      return FAILURE;
    }

    // Write to device attribute
    if ( action ) {
      printf("Writing to %s : %s\n", attribute, action);
      length = write( fd , action, sdslen(action) );
      if ( length < 0 ) {
        perror( "unable to write to attribute" );
        *reply = sdsnew("**output error**");
        close( fd );
        return FAILURE;
      }

    // read from device attribute
    } else { 
      *reply = sdscatprintf(sdsempty(),"%s ", attribute);
    	do {
        length = read(fd,input, sizeof(input)-1 );
        if( length >= 0 ) {
          input[length+1] = 0;
          *reply = sdscat(*reply,input);
        }
      }while (length == sizeof(input)-1 );

      if ( length < 0 ) {
    		perror("Failed to read attribute");
        *reply = sdscat(*reply,"**input error**");
        close( fd );
        return FAILURE;
      }  
    } 

   	close(fd);
    sdsfree(file_path);
  }

  return SUCCESS;
}

