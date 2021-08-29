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

void print_hid_device_info(struct hid_device_info * dev_info){
  printf("Device info:\n");
  printf("  Vendor: %04X:%04X\n",dev_info->vendor_id, dev_info->product_id);
  printf("  Path: %s\n",dev_info->path);
  printf("  Serial number: %ls\n",dev_info->serial_number);
  printf("  Release number: %X\n",dev_info->release_number);
  printf("  Manufacturer_string: %ls\n",dev_info->manufacturer_string);
  printf("  Interface number %d\n",dev_info->interface_number);
  printf("  Product_string: %ls\n",dev_info->product_string);
  printf("  next: %p\n",(void*)dev_info->next);
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
 
  if ( info )
    print_hid_device_info(device);
  
  while (device ) {
    do {
      if( !device->path )
        break;
      
      if (!device->product_string ) // Claimed already by other process
        break;

      // Match criterias
      if ( serial_number
        && !device->serial_number
        && swprintf(wstr, sizeof(wstr), L"%hs", serial_number)
        && !wcscmp(wstr, device->serial_number) ) 
          break;

      if ( manufacturer_string
        && !device->manufacturer_string
        && swprintf(wstr, sizeof(wstr), L"%hs", manufacturer_string)
        && !wcscmp(wstr, device->manufacturer_string) ) 
          break;

      if ( path && strcmp( path, device->path ) )
        break;

      returned_list = returned_list->next = malloc(sizeof(struct hid_device_info));
      memcpy(returned_list,device,sizeof(struct hid_device_info));
      returned_list->next = NULL;
    } while (0);

    device = device->next;
  }
  //hid_free_enumeration(device);

  if ( empty_record.next == &empty_record) 
    return NULL;
  return empty_record.next;
}

/* 
  probe for HID USB devices that match relay drivers.
  When matched, add aan entry to the device list.
*/  
int probe_hidusb(int si_index, struct _device_identifier id, GSList **device_list){

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
    if ( info ) printf("Found device at %s",hid_device->path);

    // recognize a device
    for(sdl_index = 0; supported_interface[si_index].device[sdl_index].name; sdl_index++ ){
      supported_device = &supported_interface[si_index].device[sdl_index];
      if ( supported_device->recognize && supported_device->recognize(sdl_index, hid_device ) )
        break;
    }

    // Add entry to list of active devices, if recognized        
    if ( supported_interface[si_index].device[sdl_index].name ) {
      struct _device_list *entry;
      struct stat stat_buffer;
      struct group group_buffer, *group_pointer = NULL;
      struct udev_device * usb_dev;

      // Create a new entry in active device list
      entry = malloc(sizeof(struct _device_list)); 
      entry->name = sdsnew(supported_device->name);
      // Find device path

      entry->id = sdscatprintf(sdsempty(),
                "hidusb#%04X:%04X:%ls:%ls#%s#%s",
                hid_device->vendor_id,
                hid_device->product_id,
                hid_device->serial_number ? : L"",
                hid_device->manufacturer_string ? : L"",
                hid_device->path ? : "",
                "/dev/<something>"
      );

      /*
      list = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(node, list) 
    {
        char *str = NULL;
        path = udev_list_entry_get_name(node);
        dev = udev_device_new_from_syspath(udev, path);
        if  (str = strstr(path, REQUESTED_USB_PORT))
        {
             if (str = strstr(str, "event"))
             {
                  dev_path = strdup(udev_device_get_devnode(dev));
                  udev_device_unref(dev);
                  break;
             }
        }
        udev_device_unref(dev);
    }
    */
      entry->path = sdsnew(hid_device->path ? : "");
      if ( !stat(hid_device->path, &stat_buffer) ) {
        struct group *grp;        
        grp = getgrgid(stat_buffer.st_gid);
        entry->group = sdsnew(grp->gr_name);
      } else {
          entry->group = "unknown";
      }
      entry->action = supported_device->action;
      *device_list = g_slist_append(*device_list, entry);

      if ( info ) 
        printf(" -- Recognized as %s\n",entry->name);
      
    } else if ( info ) 
      printf(" -- Not recognized\n");

    hid_device = hid_device->next; 
  }
  hid_free_enumeration(first_hid_device);
  return SUCCESS;
}   

