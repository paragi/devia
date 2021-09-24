/* 
  One-wire device interaction

  Using  SysFs

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
#include <glib.h>
#include <libgen.h>

/* Application */
#include "toolbox.h"
#include "common.h"

#include "w1.h"

#define W1_SYS_DIR (char *)"/sys/devices/w1_bus_master1"
/* 
  probe for !-wire devices 
*/  
int probe_w1(int si_index, struct _device_identifier id, GList **device_list){
  struct _device_list *entry;
  sds path = NULL;
  GList * path_list = NULL, *iterator = NULL;;
  struct dirent *dp;
  DIR *dir;

  assert(supported_interface[si_index].name);

  // Check that /sys/class exists
  if ( access(W1_SYS_DIR, F_OK) ) {
    if ( info ) printf("No one-wire SysFs entry\n");
    return FAILURE;
  }
  if ( id.device_id ) {
    struct dirent *dp;
    DIR *dir;
    sds filename;

    path_list = g_list_append(path_list, id.device_id);

    // List attributes
    if( info ) {
      path = sdscatprintf(sdsempty(),"%s/%s",W1_SYS_DIR, id.device_id);
      dir = opendir(path);
      if (dir) {
        printf("%s attributes:\n",id.device_id);  
        while ((dp = readdir(dir)) != NULL) {
          if ( !strcmp(dp->d_name, ".") != 0 || !strcmp(dp->d_name, "..") ) 
            continue;

          if ( dp->d_type != DT_REG )  
            continue;

          filename = sdscatprintf(sdsempty(),"%s/%s",path,dp->d_name);
          printf("  %s  %s\n", dp->d_name, file_permissions_string(filename));
          sdsfree(filename);
        }
        sdsfree(path); 

        puts("");
        closedir(dir);
      }
    }

  // find dir  
  } else {
    dir = opendir(W1_SYS_DIR);
    if (!dir){
      if ( info ) printf("No path to one-wire  SysFs kernel driver");
      return FAILURE;
    }

    while ((dp = readdir(dir)) != NULL) {
      if ( !strcmp(dp->d_name, ".") != 0 || !strcmp(dp->d_name, "..") ) 
        continue;

      if ( dp->d_type != DT_DIR )  
        continue;

      // Only numeric names are devices
      if ( dp->d_name[0] > '9' || dp->d_name[0] < '0' )
        continue;

      if ( info ) printf(" found %s\n",dp->d_name);
      path_list = g_list_append(path_list, sdsnew(dp->d_name));  
    }
    closedir(dir);
  }

  for ( iterator = path_list; iterator; iterator = iterator->next) {
    // Create a new entry in active device list
    entry = (struct _device_list *) malloc(sizeof(struct _device_list)); 
    memset(entry, 0, sizeof(struct _device_list));
    *device_list = g_list_append(*device_list, entry); 

    entry->name = sdsnew((char *)"One-wire device");
    entry->id = sdscatprintf( sdsempty(), "w1#%s", (char *)iterator->data );
    entry->path = sdscatprintf( sdsempty(), "%s/%s",W1_SYS_DIR, (char *)iterator->data );
    entry->group = file_permissions_string( entry->path );
    entry->action = action_w1;
  }
  return SUCCESS;
}
 

int action_w1(struct _device_list *device, sds attribute, sds action, sds *reply){
  struct stat stat_buffer;
  int fd;
  int length;
  char input[1025];

  if ( info )
    printf("w1 on: %s  Action: %s id: %s\n",attribute, action, device->id);

  // Check that device id is a valid direstory in SysFs
  if (stat(device->path, &stat_buffer) ){
    perror(device->path);
    return FAILURE;
  }

  if( !S_ISDIR(stat_buffer.st_mode)) {
    fprintf(stderr, "%s is not a valid path to a SysFS device\n", device->path);
    return FAILURE;
  }

  // List attributes
  if ( attribute ) {
    sds file_path = sdscatprintf(sdsempty(), "%s/%s",device->path, attribute); 
    
    // Check permissions
    sds permission_needed = file_permission_needed(file_path, action ? W_OK : R_OK );
    if ( sdslen(permission_needed) ){
      printf("Access denied. %s\n",permission_needed);
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
      length = write( fd , action, sdslen(action) );
      if ( length < 0 ) {
        perror( "unable to write to attribute" );
        *reply = sdsnew("**output error**");
        close( fd );
        return FAILURE;
      } else {
        *reply = sdscatprintf(sdsempty(),"%s %s",attribute,action);
      }

    // read from device attribute
    } else { 
      *reply = sdscatprintf(sdsempty(),"%s ", attribute);
    	do {
        length = read(fd,input, sizeof(input)-1 );
        if( length >= 0 ) {
          sds data;
          input[length] = 0;
          data = sdstrim(sdsnew(input),NULL);
          *reply = sdscat(*reply,data);
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
  
  // List attributes
  } else {
    struct dirent *dp;
    DIR * dir = opendir(device->path);

    if (dir) {
      while ((dp = readdir(dir)) != NULL) {
        if ( !strcmp(dp->d_name, ".") != 0 || !strcmp(dp->d_name, "..") ) 
          continue;

        if ( dp->d_type != DT_REG )   
          continue;

        *reply = sdscatprintf(*reply," %s", dp->d_name);
      }
      puts("");
      closedir(dir);
    } 
  }

  return SUCCESS;
}

