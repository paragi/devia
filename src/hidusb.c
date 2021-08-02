/* 
Driver for Nuvoton 8-16 channel USB-HID relay controller

This works with the Nuvoton relay controler.

However the device->path changes when device is unpluged and reattached. 
A stable path is needed for identificatio, since it has no serial number.

*
* Description:
*   usb hid relay controller board, from Nuvoton - Winbond Electronics Corp.
*   This 8-16-channel relay USB controller is almost identical to the Sainsmart
*   16-channel controller, except that the state of the relays are in bit-order
*   from 0 to 16, LSB first
*  
*   It's a Chinese product with precious little and useless documentation and support.
*   <https://www.cafago.com/en/p-e1812-1.html>
*
*   The Nuvoton relay controller is almost identical to the SainSmart 16 channel controler,
*   With a single exception of the bit order of the individual relays. (Gathered from the code)
*
* Identifying devices:
*   The Nuvoton device has no serial number. The only way to uniquely identify multiple devices
*   connected, is port paths. 
*   The HIDAPI enumeration returns a device node path. As the device node changes, when reconnected,
*   Its unreliable as identification.
*   As HIDAPI seem to be unsupported as of 2021, the workaround is to write a more 
*   detailed enumeration function, using the underlying UDEVB lib.
*
* Implementation:  
*   This driver returns a unique identifier, based on <vendor id>:<product id>:[<port path>:serial number>]:<Manufacturer_string>
*   the identifier is returned in the serial number string.
*   When used for reading and writing, the path is looked up.
*
* Quirk:
*   HIDAPI use a device path in the form <bus number 4 digit>:<device node>:<serial number> ("%04x:%04x:00")
*   The last two sections of  device_path, contains busnumber and device node, used by the HIDAPI to open the device
*   This is not at static identification.
*
* Quirk:
*   The Nuvoton relay controller has a 16 bit relay state.
*   When reading relay state, the byte order is MSB, LSB  (Big endian)
*   When setting the relay state, the byte order is LSB, MSB (Little endian)

NB: including source code for HIDAPI (libusb version) until version with stabel parth to device, is in curculation.

* By: Simon Rigét @ Paragi 2021
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

/* Unix */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <poll.h>

/* Linux */
#include <hidapi/hidapi.h>

/* Application */
#include "toolbox.h"
#include "common.h"
#include "relay_nuvoton.h"

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

      print_hid_device_info(device);
      
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
int probe_hidusb(struct _device_identifier id, struct _device_list ** device){

  struct hid_device_info *hid_device, *first_hid_device;
  sds * sds_array;
  int length;
  int vendor_id = 0, product_id = 0;
  sds serial_number = sdsempty();
  sds manufacturer_string = sdsempty();

  if( info )
    printf("probing HID-USB devices( %s)\n", id.device_id ? : "empty");

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
    if ( info ) printf("  Found device at %s",hid_device->path);
    do {
      if ( (recognize_nuvoton(hid_device, *device )) )
        break;
      // Put new hid device recognizers here

    } while( 0 );

    // Add generic information to the newly created list entry
    if ( device && *device ) {
      (*device)->id = sdscatprintf(sdsempty(),
                "hidusb&%04X:%04X&%s&%ls&%ls",
                hid_device->vendor_id,
                hid_device->product_id,
                hid_device->path ? : "",
                hid_device->serial_number ? : L"",
                hid_device->manufacturer_string ? : L""
      ); 
      (*device)->group = "dailout";
      device = &(*device)->next; 
      if ( info ) 
        printf("-- Recognized as %s\n",(*device)->name);

    } else if ( info ) 
      printf("-- Not recognized\n");

    hid_device = hid_device->next; 
  }
  hid_free_enumeration(first_hid_device);
  return SUCCESS;
}   

