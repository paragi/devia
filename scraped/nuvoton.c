/******************************************************************************
* 
* Driver for Nuvoton 8-16 channel USB-HID relay controller
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
*   When reading relay state, the byte over is MSB, LSB  (Big endian)
*   When setting the relay state, the byte order is LSB, MSB (Little endian)

* in /etc/udev/rules.d
* create or edit at file named 
* SUBSYSTEM=="hidraw", ATTRS{idVendor}=="0416", ATTRS{idProduct}=="5020", MODE="0660" GROUP="gpio"
*
* By: Simon Rigét @ Paragi 2021
* 
* 
*****************************************************************************/ 

/******************************************************************************
* Protocol description
* ==================================
* 
* Read relay state request repport in hex:
*   D2 10 11 11 11 11 11 11 11 11 'H' 'I' 'D' 'C' CS CS
* 
*   0:      0xD2: Read
*   1:      0x10: repport length in bytes
*   2-9:    Must be 11
*   10-13:  Command signature (HIDC)
*   14:  Checksum LSB
*   15:  Checksum MSB
*
*   Response
* 
*   Reesponse:
*   D2 0B RS RS AA XX XX XX XX XX XX 
*   
*   0.      0xD2 
*   1.      0x0B : length
*   1.      Relay state MSB
*   2.      Relay state LSB
*   3.      0xAA: ?
*   4-10:   ?
* 
* Set relay state request repport in hex:
*   C3 10 LSB MSB 00 00 00 00 00 00 'H' 'I' 'D' 'C' CS CS
* 
*   0:      C3: Write
*   1:      0x10: repport length in bytes
    2-9:    Must be 00
*   10-13:  Command signature (HIDC)
*   14:  Checksum LSB
*   15:  Checksum MSB
*
*   No response
* 
*   
*****************************************************************************/ 
/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <errno.h>
#include <error.h>
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
#include <argp.h>
#include <libexplain/ioctl.h>
#include <libexplain/open.h>
#include <libexplain/write.h>
#include <libexplain/read.h>

/* Linux */
#include <linux/hidraw.h>
#include <linux/version.h>
#include <linux/input.h>
#include <libudev.h>
#include <linux/hidraw.h>
#include <hidapi/hidapi.h>

/* Application */
#include "nuvoton.h"


#define DEBUG
#define VENDOR_ID 0x0416
#define PRODUCT_ID 0x5020
#define NUMBER_OF_RELAYS 16
#define MANUFACTURER_STRING "Nuvoton"


#define SUCCESS 0
#define FAILURE -1

struct HID_repport 
{
  uint8_t cmd;           // command READ/WRITE  
  uint8_t len;           // message length
  uint8_t byte1;         // relay state read: MSB, write LSB
  uint8_t byte2;         // relay state read: LSB, write MSB
  uint8_t reserved[6];   // reserved bytes
  uint8_t signature[4];  // command signature
  uint8_t chk_lsb;       // LSB checksum 
  uint8_t chk_msb;       // MSB checksum 
};

// Convert int to binary string
char *int2bin(int value ,int len, char *buffer, int buf_size) {
  if(len+1 > buf_size) len = buf_size-1;
  buffer[len] = 0;
  for (int i = len-1; i >= 0; i--) 
    buffer[i] = value & (1<<(len-i-1)) ? '1' : '0';
  return buffer;
}

void print_struct(struct usb_device_info_extendet * dev_info){
  printf("Device info:\n");
  printf("  Vendor: %04X:%04X\n",dev_info->vendor_id, dev_info->product_id);
  printf("  Device node: %s\n",dev_info->device_node);
  printf("  Serial number: %s\n",dev_info->serial_number);
  printf("  Release number: %X\n",dev_info->release_number);
  printf("  Manufacturer_string: %s\n",dev_info->manufacturer_string);
  printf("  Product_string: %s\n",dev_info->product_string);
  printf("  Interface number %d\n",dev_info->interface_number);
  printf("  Port name: %s\n",dev_info->port);
  printf("  Vendor name: %s\n",dev_info->vendor_name);
  printf("  next: %p\n",(void*)dev_info->next);
}

void free_usb_device_info_subelements(struct usb_device_info_extendet * this){
  free(this->device_node);
  free(this->serial_number);
  free(this->manufacturer_string);
  free(this->product_string);
  free(this->port);
  free(this->vendor_name );
}

void free_enumerate_usb_devices(struct usb_device_info_extendet * this){
  while( this != NULL){
    struct usb_device_info_extendet * next = this->next;
    free_usb_device_info_subelements(this);
    free(this);
    this = next;
  }
}  

struct usb_device_info_extendet * enumerate_usb_devices(
  unsigned int vendor_id, 
  unsigned int product_id, 
  char * serial_number, 
  char * port, 
  char * manufacturer_string
){
  struct udev *udev;
  struct udev_enumerate *enumerate;
  struct usb_device_info_extendet *dev_info, *first_entry, *previous_entry;
  struct udev_list_entry *devices, *entry;
  struct udev_device *parent_dev;
  
  udev = udev_new();
  if (!udev) {
    fprintf(stderr, "udev_new() FAILURE\n");
    return NULL;
  }

  enumerate = udev_enumerate_new(udev);
  if (!udev) {
    fprintf(stderr, "udev_enumerate_new() FAILURE\n");
    return NULL;
  }
  udev_enumerate_add_match_subsystem(enumerate, "usb");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);

  first_entry = previous_entry = dev_info = malloc(sizeof(struct usb_device_info_extendet));
  dev_info->next = NULL;

  udev_list_entry_foreach(entry, devices) {
    const char * strp;
    const char* path;
    struct udev_device* dev;

    if (!dev_info) {
      fprintf(stderr, "Out of memory!\n");
      exit(-1);
    }
    path = udev_list_entry_get_name(entry);
    dev = udev_device_new_from_syspath(udev, path);
    if (dev){
      if( udev_device_get_devnode(dev)){
        // populate the device info structure
        strp = udev_device_get_devnode(dev); 
        dev_info->device_node = strdup(strp && strp[0] ? strp : ""); // Free 
        strp = udev_device_get_sysattr_value(dev, "idVendor");
  			dev_info->vendor_id = strp ? strtoul(strp,NULL,16) & 0xFFFF : 0;
        strp = udev_device_get_sysattr_value(dev, "idProduct");
  			dev_info->product_id = strp ? strtoul(strp,NULL,16) & 0xFFFF : 0;
        strp = udev_device_get_sysattr_value(dev, "serial_number");
  			dev_info->serial_number = strdup(strp && strp[0] ? strp : ""); // Free 
        strp = udev_device_get_sysattr_value(dev, "bcdDevice");
  			dev_info->release_number = strp && strp[0] ? strtol(strp, NULL, 16): 0x0; 
        strp = udev_device_get_sysattr_value(dev, "manufacturer");
  			dev_info->manufacturer_string = strdup(strp && strp[0] ? strp : ""); // Free 
        strp = udev_device_get_sysattr_value(dev, "product");
  			dev_info->product_string = strdup(strp && strp[0] ? strp : ""); // Free 
  			dev_info->usage_page = 0;
  			dev_info->usage = 0;
        // This might not work ??
				parent_dev = udev_device_get_parent_with_subsystem_devtype(dev,"usb",NULL);
  			strp = parent_dev ? udev_device_get_sysattr_value(parent_dev, "bInterfaceNumber") : NULL;
  			dev_info->interface_number = strp ? strtol(strp, NULL, 16): -1;

        // Extensions
        strp = udev_device_get_sysname(dev); 
        dev_info->port = strdup(strp && strp[0] ? strp : ""); // Free 
        strp = udev_device_get_property_value(dev, "ID_VENDOR_FROM_DATABASE");
        dev_info->vendor_name = strdup(strp && strp[0] ? strp : ""); // Free 

        // Match
        if(  (!vendor_id || vendor_id == dev_info->vendor_id )
          && ( !product_id || product_id == dev_info->product_id )  
          && ( !serial_number || !serial_number[0] || ( dev_info->serial_number && !strcmp(serial_number,dev_info->serial_number) ) )
          && ( !port || !port[0] || ( dev_info->port && !strcmp(port,dev_info->port) ))
          && ( !manufacturer_string || !manufacturer_string[0]|| ( dev_info->manufacturer_string && !strcmp(manufacturer_string,dev_info->manufacturer_string) ) ) 
        ) {
          // populate list
          dev_info->next = malloc(sizeof(struct usb_device_info_extendet));
          previous_entry = dev_info;
          dev_info = dev_info->next;
          dev_info->next = NULL;
        }else{
          free_usb_device_info_subelements(dev_info);
        }  
      } 
      udev_device_unref(dev);
    }
  }

  previous_entry->next = NULL;
  if(dev_info == first_entry) 
    first_entry = NULL;
  free(dev_info);

  udev_enumerate_unref(enumerate);
  udev_unref(udev);

  return first_entry;
}

int open_device(char *path){
	int device_descriptor;
 	int res, desc_size = 0;
	struct hidraw_report_descriptor rpt_desc;

  device_descriptor = open(path, O_RDWR|O_NONBLOCK);
  if (device_descriptor <= 0) {
    fprintf(stderr, "Unable to open device: %s\n", explain_open(path,0, O_RDWR|O_NONBLOCK));
		return FAILURE;
	}

	// Get the report descriptor 
	memset(&rpt_desc, 0x00, sizeof(rpt_desc));

	if ( ioctl(device_descriptor, HIDIOCGRDESCSIZE, &desc_size) < 0 ) {
    fprintf(stderr,"Unable to read HID repport descriptor size:  %s\n",explain_ioctl(device_descriptor, HIDIOCGRDESCSIZE, &desc_size));
    return device_descriptor;
  } 
  printf("Repport descriptor size: %d\n",desc_size);
	rpt_desc.size = desc_size;
  if( ioctl(device_descriptor, HIDIOCGRDESC, &rpt_desc) < 0 ) {
    fprintf(stderr,"Unable to read HID repport descriptor:  %s\n",explain_ioctl(device_descriptor, HIDIOCGRDESC, &rpt_desc));
    return device_descriptor;
  }
  printf("Repport descriptor read\n");

  return device_descriptor;
}
/**********************************************************
 * Function detect_relay_card_nuvoton()
 * 
 * Description: Detect Nutoton 8-16 channel relay controler
 * 
 * Function is called in two situations:
 * 1. to enumerate alle available devices. In this case the linked list relay_info is filled with entries.
 * 2. To find a specific device, and return the device node and number of relays controled. (relay_info = NULL)
 * 
 * Parameters: 
 *  portname:       pointer to path to device node  
 *  num_relays:     pointer to number of relays
 *  serial:         string to identify device (vendor_id:product_id:port device_node:serial number)
 *  relay_info:     linked list of deviceses
 * 
 * Return:  0 - SUCCESS
 *         -1 - fail, no relay card found
 * 
 * NB: Relay_info is already allocated and points to the first empty element of the list.
 *     New elements are written into it, and a new empty element with the next pointing to null, i added.
 *     The relay_info pointer returned, must point to the last empty element, with next = NULL as an end marker.
 *********************************************************/
/*
int detect_relay_controller_nuvoton(char* portname, uint8_t* num_relays, char* serial, relay_info_t **relay_info){
  struct usb_device_info_extendet * dev_info, * device_info = NULL;
  char serial_number[sizeof((*relay_info)->serial)] =""; 
  char port[sizeof((*relay_info)->serial)] =""; 
  char manufacrurer_string[256] = MANUFACTURER_STRING;
  char *strp = NULL;
  char *p1 = NULL, *p2 = NULL, *p3 = NULL;

  #ifdef DEBUG
    relay_info_t *rip;
    if(relay_info){
      rip = *relay_info;
      printf("Detect Nuvoton: Port: %s serial: %s, info: %p\n", portname, serial, *relay_info);
    }
  #endif

  if(!relay_info && !serial){
    printf("No serial number given\n");
      return FAILURE;
  }

  // Request device node of specific device 
  if( !relay_info && serial )
  {
    for(int i=1; (strp = strsep(&serial,":")) != NULL; i++){
      switch(i){
        case 1: 
          if( (strp && strp[0] ? strtol(strp, NULL, 16) : 0) != VENDOR_ID)
            return FAILURE;
          break;
        case 2:
          if( (strp && strp[0] ? strtol(strp, NULL, 16) : 0) != PRODUCT_ID)
            return FAILURE;
          break;
        case 3:  
          if(strp[0])  snprintf(port,sizeof(port),"%s",strp);
          break;
        case 4:  
          if(strp[0])  snprintf(serial_number,sizeof(serial_number),"%s",strp);
          break;
        case 5:
          if(strp[0])  snprintf(manufacrurer_string,sizeof(manufacrurer_string),"%s",strp);
          break;
      };
    }
  }

  #ifdef DEBUG
    printf("Nuvton: enumerate_usb_devices  %04X:%04X serial_number:%s port:%s manufacrurer_string:%s :\n",
      VENDOR_ID, PRODUCT_ID, serial_number, port, manufacrurer_string);
  #endif

  device_info = dev_info = enumerate_usb_devices(VENDOR_ID, PRODUCT_ID, serial_number, port, manufacrurer_string);
  
  // Copy to relay_info structure
  while( dev_info && relay_info && (*relay_info) ){
    (*relay_info)->relay_type = NUVOTON_USB_RELAY_TYPE;
    snprintf( (*relay_info)->serial,
            sizeof((*relay_info)->serial)-1,
            "%04X:%04X:%.20s:%.32s:%.32s",
            dev_info->vendor_id,
            dev_info->product_id,
            dev_info->port,
            dev_info->serial_number,
            dev_info->manufacturer_string
    ); 
   
    #ifdef DEBUG
      print_struct(dev_info);
    #endif  

    // create a new instance of relay_info
    (*relay_info)->next = malloc(sizeof(relay_info_t));  
    assert((*relay_info)->next);
    *relay_info = (*relay_info)->next;
    (*relay_info)->next = NULL;

    dev_info = dev_info->next;
  }

  // Return parameters
  if( num_relays != NULL) *num_relays = 16;

  // Quirk:
  // HIDAPI use a device path in the form <bus number 4 digit>:<device node>:<serialnumber> ("%04x:%04x:00")
  // The last two sections of device_path, contains busnumber and device node, used by the HIDAPI to open the device
  if( portname ) 
  { 
    p3 = strtok(device_info->device_node, "/");
    while(p3)
    {
      p1 = p2;
      p2 = p3;
      p3 = strtok(NULL, "/");
    }
    sprintf(portname,"%04x:%04x:00",atoi(p1),atoi(p2));
  }

  free_enumerate_usb_devices(device_info);

  #ifdef DEBUG
    if(portname) printf("Returning device node: %s\n",portname);
    if(relay_info){
      printf("Added to relay_info list:\n");
      for(int i = 1 ; rip->next; i++, rip = rip->next)
        printf("    #%d type: %d serial: %s next: %p\n",i,rip->relay_type,rip->serial,rip->next);
    }
  #endif      
   
  return SUCCESS;
}
*/
static int get_relay_state(hid_device *device_descriptor, uint16_t *states)
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


  if (hid_write(device_descriptor, (unsigned char *)&hid_msg, sizeof(hid_msg)) < 0)
    fprintf(stderr, "Unable to write to device\n");
    return FAILURE;

  /*  
  if (write(device_descriptor, (void *)&hid_msg, sizeof(hid_msg)) <= 0) {
    fprintf(stderr, "Unable to write to device: %s\n", explain_write(device_descriptor, (void *)&hid_msg, sizeof(hid_msg)));
    return FAILURE;
  }*/

  // Read response
  memset((unsigned char *)&hid_msg,0,sizeof(hid_msg));
  if(hid_read(device_descriptor, (unsigned char *)&hid_msg, sizeof(hid_msg)) <=0 ) {
    fprintf(stderr, "Unable to read device\n");
    return FAILURE;
  }
  /*
  if (read(device_descriptor, (void *)&hid_msg, sizeof(hid_msg)) <= 0)
    return FAILURE;
  */
  // Big endian
  *states = hid_msg.byte2 + (hid_msg.byte1 << 8);

  #ifdef DEBUG
    printf("Recieved HID repport from device: "); 
    for (i=0; i<sizeof(struct HID_repport); i++) 
      printf("%02X ", *(((uint8_t*)&hid_msg)+i)); 
    printf("\n"); 
   printf("Relay state = 0x%04x\n", *states); 
  #endif

  return SUCCESS;
}


static int set_relays(hid_device *handle, uint16_t states) 
{
  struct HID_repport  hid_msg;
  int i;
  uint16_t checksum=0;

  // Create HID repport set relays Request
  memset(&hid_msg, 0x00, sizeof(struct HID_repport));
  // Little endian
  hid_msg.byte1 = states & 0x00ff;
  hid_msg.byte2 = (states & 0xff00) >> 8;
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
    printf("Set relays = 0x%04x\n", states);
  #endif

  if (hid_write(handle, (unsigned char *)&hid_msg, sizeof(hid_msg)) < 0)
    return FAILURE;

  return SUCCESS;
}

/**********************************************************
 * Function get_relay_nuvoton()
 * 
 * Description: Get the current relay state
 * 
 * Parameters: portname (in)     - communication port
 *             relay (in)        - relay number
 *             relay_state (out) - current relay state
 * 
 * Return:   0 - SUCCESS
 *          <0 - fail
 *********************************************************/
/*
int get_relay_nuvoton(char* portname, uint8_t relay, relay_state_t* relay_state, char* serial)
{
  hid_device *hid_dev;
  uint16_t relay_states;

  if (relay<1 || relay>NUMBER_OF_RELAYS)
  {  
    fprintf(stderr, "ERROR: Relay number out of range\n");
    return FAILURE;
  }

  if (!(hid_dev = hid_open_path(portname)))
  {
    fprintf(stderr, "unable to open HID API device %s\n", portname);
    return FAILURE;
  }

  if (get_relay_state(hid_dev, &relay_states) )
  {
    fprintf(stderr, "unable to read data from device %s (%ls)\n", portname, hid_error(hid_dev));
    return FAILURE;
  }

  *relay_state = relay_states & ( 1 << (relay-1)) ? ON : OFF;

  #ifdef DEBUG
    char str[20];
    printf("Relays : %s\n",int2bin(relay_states, 16, str, sizeof(str)));
    printf("Relay %d is %s\n", relay, *relay_state ? "ON" : "OFF");
  #endif

  hid_close(hid_dev);
  return 0;
}
*/
/**********************************************************
 * Function set_relay_nuvoton()
 * 
 * Description: Set new relay state
 * 
 * Parameters: portname (in)     - communication port
 *             relay (in)        - relay number
 *             relay_state (in)  - current relay state
 * 
 * Return:   0 - SUCCESS
 *          <0 - fail
 *********************************************************/
/*
int set_relay_nuvoton(char* portname, uint8_t relay, relay_state_t relay_state, char* serial)
{ 
  hid_device *hid_dev;
  uint16_t   relay_states;
  
  if (relay > NUMBER_OF_RELAYS)
  {  
    fprintf(stderr, "ERROR: Relay number out of range\n");
    return FAILURE;      
  }

  if ((hid_dev = hid_open_path(portname)) == NULL)
  {
    fprintf(stderr, "unable to open HID API device %s\n", portname);
    return FAILURE;      
  }

  if (get_relay_state(hid_dev, &relay_states) < 0)
  {
    fprintf(stderr, "unable to read data from device %s (%ls)\n", portname, hid_error(hid_dev));
    return FAILURE;      
  }

  #ifdef DEBUG
    char str[20];
    printf("Relays before: %s\n",int2bin(relay_states,16,str,sizeof(str)));
  #endif
  
  // Set bit coorsponding to relay, to relay_state (off = 0 on = 1)
  relay_states = (relay_states & ~(1<<(relay-1))) | (relay_state << (relay-1));
  
  #ifdef DEBUG
    printf("Relays after : %s\n",int2bin(relay_states,16,str,sizeof(str)));
  #endif

  if (set_relays(hid_dev, relay_states) )
  {
    fprintf(stderr, "unable to write data to device %s (%ls)\n", portname, hid_error(hid_dev));
    return FAILURE;
  }

  hid_close(hid_dev);
  return SUCCESS;
}
*/

int main(char **argv, int argc){

  struct usb_device_info_extendet *device_info = NULL;
 	int device_descriptor;
  char port[256] = ""; 
  char manufacrurer_string[256] = MANUFACTURER_STRING;
  char *strp = NULL;
  char *p1 = NULL, *p2 = NULL, *p3 = NULL;
  uint16_t states;
  struct hidraw_devinfo info;
  hid_device *hid_dev; 

  hid_init();
  device_info = enumerate_usb_devices(0x0416,0x5020, NULL, NULL, NULL);

  while( device_info){
    print_struct(device_info);
    // Get relay states
    if (!(hid_dev = hid_open_path(device_info->device_node))) {
      fprintf(stderr, "unable to open HID API device %s\n", device_info->device_node);
      return FAILURE;
    }
    get_relay_state(hid_dev, &states);
    /*
    device_descriptor = open_device(device_info->device_node);
    if( device_descriptor >0 ){
      get_relay_state(device_descriptor, &states);
      close(device_descriptor);
    }
    */
    hid_close(hid_dev);
    device_info = device_info->next;
  }  
  free_enumerate_usb_devices(device_info);

  return SUCCESS;
}