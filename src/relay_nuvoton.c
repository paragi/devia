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

#define DEBUG
// #define TEST

struct HID_repport 
{
  uint8_t  cmd;          // command READ/WRITE  
  uint8_t  len;          // message length
  uint8_t  byte1;        // relay state read: MSB, write LSB
  uint8_t  byte2;        // relay state read: LSB, write MSB
  uint8_t  reserved[6];  // reserved bytes
  uint8_t  signature[4]; // command signature
  uint8_t  chk_lsb;      // LSB checksum 
  uint8_t  chk_msb;      // MSB checksum 
};


static int get_nuvoton(hid_device *handle, int *relay_state) 
{
  int i;
  struct HID_repport hid_msg;
  unsigned int checksum=0;
  
  // Create HID repport read status Request
  memset(&hid_msg, 0x11, sizeof(struct HID_repport));
  hid_msg.cmd = 0xD2;
  hid_msg.len = sizeof(struct HID_repport) - 2;
  memcpy(hid_msg.signature, "HIDC", 4);

  // Check sum
  for(i=0; i<hid_msg.len; i++) 
    checksum += *(((uint8_t*)&hid_msg)+i);
  hid_msg.chk_lsb = checksum & 0x00ff;
  hid_msg.chk_msb = (checksum & 0xff00) >> 8;

  #ifdef DEBUG
    printf("Sending HID repport to device:    "); 
    for (i=0; i<sizeof(struct HID_repport); i++) printf("%02X ", *(((uint8_t*)&hid_msg)+i)); 
     printf("\n"); 
  #endif

  if (hid_write(handle, (unsigned char *)&hid_msg, sizeof(hid_msg)) < 0)
    return FAILURE;

  // Read response
  memset((unsigned char *)&hid_msg,0,sizeof(hid_msg));
  if (hid_read(handle, (unsigned char *)&hid_msg, sizeof(hid_msg)) < 0)
    return FAILURE;

  // Big endian
  *relay_state = hid_msg.byte2 + (hid_msg.byte1 << 8);

  #ifdef DEBUG
    printf("Recieved HID repport from device: "); 
    for (i=0; i<sizeof(struct HID_repport); i++) 
      printf("%02X ", *(((uint8_t*)&hid_msg)+i)); 
    printf("\n"); 
   printf("Relay state = 0x%04x\n", *relay_state); 
  #endif

  return SUCCESS;
}

static int set_nuvoton(hid_device *handle, int *relay_state) {
  struct HID_repport  hid_msg;
  int i;
  uint16_t checksum=0;

  // Create HID repport set relays Request
  memset(&hid_msg, 0x00, sizeof(struct HID_repport));
  // Little endian
  hid_msg.byte1 = *relay_state & 0x00ff;
  hid_msg.byte2 = (*relay_state & 0xff00) >> 8;
  hid_msg.cmd = 0xC3;
  hid_msg.len = sizeof(struct HID_repport) - 2;
  memcpy(hid_msg.signature, "HIDC", 4);

  // Create checksum
  for (i=0; i<hid_msg.len; i++) checksum += *(((uint8_t*)&hid_msg)+i);
  hid_msg.chk_lsb = checksum & 0x00ff;
  hid_msg.chk_msb = (checksum & 0xff00) >> 8;

  #ifdef DEBUG
    printf("Sending HID repport to device:    "); 
    for (i=0; i<sizeof(struct HID_repport); i++) printf("%02X ", *(((uint8_t*)&hid_msg)+i)); 
      printf("\n"); 
    printf("Set relays = 0x%04x\n", *relay_state);
  #endif

  if (hid_write(handle, (unsigned char *)&hid_msg, sizeof(hid_msg)) < 0)
    return FAILURE;

  return SUCCESS;
}

int action_nuvoton(struct _device_list *device, sds attribute, sds action, sds *reply) {
  int relay_id = 0;
  int relay_state = -1;
  int return_code = SUCCESS;
  hid_device *handle;

  if ( (handle = hid_open_path(device->path)) > 0) {
    perror("Unable to open HID API device");
    return FAILURE;      
  }

  if( attribute && strcmp(strtolower(attribute),"all") )
    relay_id = strtol(attribute,NULL,10);  

  return_code = get_nuvoton(handle, &relay_state); 

  if (action) {
    int mask = attribute ? 1<<(relay_id - 1) : 0xff;
    if (!strcmp(strtolower(action), "off") )
      relay_state &= ~mask; 
    if (!strcmp(strtolower(action), "on") )
      relay_state |= mask; 
    if (!strcmp(strtolower(action), "toggle") )
      relay_state ^= ~relay_state & mask;
    return_code = set_nuvoton(handle, &relay_state); 
  } 

  if(attribute > 0 )
    *reply = (1<<relay_id) & relay_state ? sdsnew("on") : sdsnew("off");
  else      
    *reply = sdsint2bin(relay_state + 0LL,16);

  hid_close(handle);

  return return_code;
}

// The interface scanner, asks if this is your device
int recognize_nuvoton(int sdl_index, void *dev_info) {
  struct hid_device_info *hid_device_info = dev_info; 

  if ( hid_device_info
    && hid_device_info->vendor_id == 0x0416
    && hid_device_info->product_id == 0x5020
    && !wcscmp(hid_device_info->manufacturer_string,L"Nuvoton" )){
 
    return true;     
  }

  return false;
}