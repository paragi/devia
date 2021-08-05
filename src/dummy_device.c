/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

/* Linux */
#include <glib.h>

/* Application */
#include "toolbox.h"
#include "common.h"

// Dummy interface
int probe_dummy(int si_index, struct _device_identifier id, GSList **device_list){
  int sdl_index;
  const struct _supported_device * supported_device;
  struct _device_list *entry;
  assert(supported_interface[si_index].name);
     
  // make two device entries
  for(int i = 1; i <3; i++) {
    if ( info ) 
      printf("Found a dummy device ");

    // recognize a device
    for(sdl_index = 0; supported_interface[si_index].device[sdl_index].name; sdl_index++ ){
      supported_device = &supported_interface[si_index].device[sdl_index];
      if ( supported_device->recognize && supported_device->recognize( sdl_index, NULL ) )
        break;
    }

    // Add entry to list of active devices, if recognized        
    if ( supported_interface[si_index].device[sdl_index].name ) {
      // Create a new entry in active device list, and push it infront of the list
      entry = malloc(sizeof(struct _device_list)); 
      entry->name   = sdscatprintf(sdsnew(supported_device->name), " - Device #%d",i);
      entry->id     = sdscatprintf(sdsempty(), "123-%d",i);
      entry->path   = sdsnew("no path");
      entry->group   = "No group";
      entry->action = supported_device->action;
      *device_list = g_slist_append(*device_list, entry);

      if ( info ) 
        printf(" -- Recognized as %s\n",entry->name);

    } else if ( info ) 
      printf(" -- Not recognized\n");
  }
  return SUCCESS;
}    


// Dummy device
int action_dummy(struct _device_list *device, sds attribute, sds action, sds *reply){
  *reply = sdscatprintf(sdsempty(),"%s = %s",attribute,action ? : "OFF-LINE");
  return SUCCESS;
}


int recognize_dummy(int sdl_index, void * dev_info ) {
  return true;     
}