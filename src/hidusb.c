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
#include <libudev.h>

/* Application */
#include "toolbox.h"
#include "common.h"

#include "hidusb.h"

#ifndef _WCHAR_T_DEFINED
// VSCode has a problem with using include paths....
typedef unsigned short wchar_t;
#endif

#define DEBUG
// #define TEST

void print_hid_device_info(struct hid_device_info * dev_info, struct _device_list *entry){
  //printf("Device info:\n");
  printf("  Vendor: %04X:%04X\n",dev_info->vendor_id, dev_info->product_id);
  printf("  Path: %s\n",dev_info->path);
  printf("  Serial number: %ls\n",dev_info->serial_number);
  printf("  Release number: %X\n",dev_info->release_number);
  printf("  Manufacturer_string: %ls\n",dev_info->manufacturer_string);
  printf("  Interface number %d\n",dev_info->interface_number);
  printf("  Product_string: %ls\n",dev_info->product_string);
  //printf("  next: %p\n",(void*)dev_info->next);
  puts("  ---");
  printf("  Device name: %s\n",entry->name);
  printf("  id: %s\n",entry->id);
  printf("  Port: %s\n",entry->port);
  if( sdslen(entry->path) ) {
    printf("  Path: %s\n",entry->path);
    printf("  Group: %s\n",entry->group);
      printf("  %s %s\n",
        file_permissions_string(entry->path),
        file_permission_needed(entry->path, X_OK)
      );
  }

}  

// Extension function for hid_enumerate(vendor_id, product_id) with extra match criterias
struct hid_device_info * hidusb_enumerate_match(
  unsigned int vendor_id, 
  unsigned int product_id, 
  char * serial_number, 
  char * manufacturer_string,
  char * path
){
  struct hid_device_info *device, *returned_list, empty_record;
  wchar_t wstr[256];

  // Make a startingpoint for linked list
  returned_list = empty_record.next = &empty_record;

  if ( !(device = hid_enumerate(vendor_id, product_id)) 
    || device->product_string == NULL 
    || device->path == NULL) { // Test with 0 ,0 
    return NULL;  
  }

  for ( ; device ; device = device->next) {
    if( !device->path )
      continue;
    
    if (!device->product_string ) // Claimed already by other process
      continue;

    // Match criterias
    if ( serial_number
      && !device->serial_number
      && swprintf(wstr, sizeof(wstr), L"%hs", serial_number)
      && !wcscmp(wstr, device->serial_number) ) 
        continue;

    if ( manufacturer_string
      && !device->manufacturer_string
      && swprintf(wstr, sizeof(wstr), L"%hs", manufacturer_string)
      && !wcscmp(wstr, device->manufacturer_string) ) 
        continue;

    if ( path && strcmp( path, device->path ) )
      continue;

    returned_list = returned_list->next = (struct hid_device_info *) malloc(sizeof(struct hid_device_info));
    memcpy(returned_list,device,sizeof(struct hid_device_info));
    returned_list->next = NULL;
  }
  //hid_free_enumeration(device);

  if ( empty_record.next == &empty_record) 
    return NULL;
  return empty_record.next;
}

// Find path to coorsponding hidraw device kernel pseudo file.
sds find_hidraw_path(char *port){
  sds device_path = sdsempty();
  GList *sys_path;
  char *path;
  struct dirent *dp;
  DIR *dir;
printf("test1: %s\n",port);    

  if ( !port || !strlen(port) )
    return device_path;
printf("test2: %s\n",port);    

  // Find path to device kernel pseudo hidraw file 
  sys_path = finddir( (char *)"/sys/devices", port);
printf("test3: %s\n",port);    
  if( !sys_path ) {
    if ( info )
      puts("  sysfs path not found");
    return device_path;
  }
printf("test4: %s\n",port);    
  if ( g_list_length(sys_path) > 1 ) {
    if ( info ) 
      puts("  too many candidates for sysfs path");
    finddir_free(sys_path);
    g_list_free(sys_path);
    return device_path;
  }

  path = strdup( (const char*)g_list_nth(sys_path, 0)->data );
  finddir_free(sys_path);
  g_list_free(sys_path);

  // Find hidraw directory and name of hidraw device
  sys_path = finddir( path, (char *)"hidraw");

  if( !sys_path ) {
    if ( info )
      puts("  sysfs path not found");
    return device_path;
  }
    
  if ( g_list_length(sys_path) > 1 ) {
    if ( info ) 
      puts("  too many candidates for sysfs path");
    finddir_free(sys_path);
    g_list_free(sys_path);
    return device_path;
  }

  path = strdup( (const char*)g_list_nth(sys_path, 0)->data );
  finddir_free(sys_path);
  g_list_free(sys_path);

  if ( !(dir = opendir(path)) ){
    if ( info )
      puts("  Unable to read system path");
    free( path );
    return device_path;
  }

  // Find directory name that starts with hidraw
  while ((dp = readdir(dir)) != NULL) {
    if ( dp->d_type != DT_DIR )  
        continue;

    if( !strncmp("hidraw",dp->d_name,6) ) {
      device_path = sdscatprintf(device_path,"/dev/%s",dp->d_name );
      break;
    }
  }
  closedir(dir);
  return device_path;
}

/* 
  probe for HID USB devices that match relay drivers.
  When matched, add aan entry to the device list.
*/  
int probe_hidusb(int si_index, struct _device_identifier id, GList **device_list){

  struct hid_device_info *hid_device, *first_hid_device;
  sds * sds_array;
  int length;
  int vendor_id = 0, product_id = 0;
  sds serial_number = sdsempty();
  sds manufacturer_string = sdsempty();
  const struct _supported_device * supported_device;

  assert(supported_interface[si_index].name);

  // Split device id argument into components
  if(id.device_id){
    sds_array = sdssplitlen(id.device_id,sdslen(id.device_id), ":", 1, &length);
    if (length >0 )  
      vendor_id =strtol(sds_array[0],NULL,16);
    if (length >1 )
      product_id =strtol(sds_array[1],NULL,16);
    if (length >2 )
      serial_number = sdsnew(sds_array[2]);  
    if (length >3 )
      manufacturer_string = sdsnew(sds_array[3]);
    sdsfreesplitres(sds_array, length); 
  }

  // Get a list of USB HID devices (Linked with libusb-hidapi) 
  first_hid_device = hid_device = hidusb_enumerate_match(vendor_id, product_id, serial_number, manufacturer_string, id.port);
  while (hid_device) {
    int sdl_index;

    if ( info ) printf("Found device at %s\n",hid_device->path);

    // call recognize function for all devices, with this interface
    for(sdl_index = 0; supported_interface[si_index].device[sdl_index].name; sdl_index++ ){
      supported_device = &supported_interface[si_index].device[sdl_index];
      if ( supported_device->recognize && supported_device->recognize(sdl_index, hid_device ) )
        break;
    }

    // Add entry to list of active devices, if recognized        
    if ( supported_interface[si_index].device[sdl_index].name ) {
      struct _device_list *entry;

      // Create a new entry in active device list
      entry = (struct _device_list *) malloc(sizeof(struct _device_list)); 
      memset(entry, 0, sizeof(struct _device_list));
      *device_list = g_list_append(*device_list, entry);

      entry->name = sdsnew(supported_device->name);
      entry->id = sdscatprintf(sdsempty(),
                "hidusb#%04X:%04X:%ls:%ls#%s",
                hid_device->vendor_id,
                hid_device->product_id,
                hid_device->serial_number ? : L"",
                hid_device->manufacturer_string ? : L"",
                hid_device->path 
      );
      entry->port = sdsnew(hid_device->path);
      entry->path = find_hidraw_path(entry->port);
      entry->group = sdsempty();
      entry->action = supported_device->action;

      if ( info ) 
        print_hid_device_info(hid_device, entry);

    } else if ( info ) 
      printf(" -- Not recognized\n");

    hid_device = hid_device->next; 
  }

  hid_free_enumeration(first_hid_device);
  return SUCCESS;
}   

