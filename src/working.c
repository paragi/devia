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

#define DEBUG

#define SUCCESS 0
#define FAILURE -1

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


// Convert int to binary string
char *int2bin(int value ,int len, char *buffer, int buf_size);

void print_hid_device_info2(struct hid_device_info * dev_info){
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
struct hid_device_info * hid_enumerate_match(
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

  if ((device = hid_enumerate(vendor_id, product_id)) == NULL) // Test with 0 ,0 
    return NULL;  

  if (device->product_string == NULL || device->path == NULL)
    return NULL;  
  
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

      print_hid_device_info2(device);
      
      returned_list = returned_list->next = (struct hid_device_info *)malloc(sizeof(struct hid_device_info));
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

static int get_relay(hid_device *handle, uint16_t *bitmap)
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
  *bitmap = hid_msg.byte2 + (hid_msg.byte1 << 8);

  #ifdef DEBUG
    printf("Recieved HID repport from device: "); 
    for (i=0; i<sizeof(struct HID_repport); i++) 
      printf("%02X ", *(((uint8_t*)&hid_msg)+i)); 
    printf("\n"); 
   printf("Relay state = 0x%04x\n", *bitmap); 
  #endif

  return SUCCESS;
}

static int set_relay(hid_device *handle, uint16_t bitmap) 
{
  struct HID_repport  hid_msg;
  int i;
  uint16_t checksum=0;

  // Create HID repport set relays Request
  memset(&hid_msg, 0x00, sizeof(struct HID_repport));
  // Little endian
  hid_msg.byte1 = bitmap & 0x00ff;
  hid_msg.byte2 = (bitmap & 0xff00) >> 8;
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
    printf("Set relays = 0x%04x\n", bitmap);
  #endif

  if (hid_write(handle, (unsigned char *)&hid_msg, sizeof(hid_msg)) < 0)
    return FAILURE;

  return SUCCESS;
}

//#define TEST

#ifdef TEST
int main(){
  struct hid_device_info *device, *first_device;

  hid_init();
  
  first_device = device = hid_enumerate_match(0x0416, 0x5020, NULL, NULL, "Nuvoton");
  
  while (device) {
    hid_device *hid_dev;
    uint16_t relay_states;
    int relay = 1;
    
    printf("  Getting %s\n",device->path);
    if ((hid_dev = hid_open_path(device->path))) {
      if (! get_relay(hid_dev, &relay_states) ) {
        char str[20];
        printf("Relays : %s  %d is %s\n",int2bin(relay_states, 16, str, sizeof(str)),relay, (1<<(relay-1)) & relay_states ? "ON":"OFF");

        // Toggle relay
        relay_states ^= 1<<(relay-1);

        if (1 || set_relay(hid_dev, relay_states) ) {
          fprintf(stderr, "unable to write data to device %s (%ls)\n", device->path, hid_error(hid_dev));
        }
        printf("Relays : %s  %d should be %s\n",int2bin(relay_states, 16, str, sizeof(str)),relay, (1<<(relay-1)) & relay_states ? "ON":"OFF");

      } else {
        fprintf(stderr, "unable to read data from device %s (%ls)\n", device->path, hid_error(hid_dev));
      }
      hid_close(hid_dev);
    } else {
      fprintf(stderr, "unable to open HID API device %s\n", device->path);
    }
    
    device = device->next;
  }
  hid_free_enumeration(first_device); 
  return 0;
}
#endif
