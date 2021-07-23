
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
#include <argp.h>

/* Unix */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <poll.h>

/* Linux */
#include <linux/hidraw.h>
#include <linux/version.h>
#include <linux/input.h>
#include <libudev.h>
#include <hidapi/hidapi.h>
#include <linux/hidraw.h>

/* Application */
#include "toolbox.h"


#define DEBUG 1

#define debug(...) do { if (DEBUG) fprintf(stderr, "Debug: "__VA_ARGS__); } while (0)

#define SUCCESS 0
#define FAILURE -1

#define BUS_USB			0x03 // udev bus type

// Extention of hidapi info structure 
struct hidraw_device_info {
  sds sysfs_path;           // kernel path to device
  sds port;                 // Add on - unique description of port, the device is attached to 
	sds device_node;          // Platform-specific device node of hidraw
  sds parent_device_node;   // parent USB device node
	unsigned short vendor_id; // Device Vendor ID
	unsigned short product_id;// Device Product ID
	unsigned short bus_type;  // 3 = USB
	sds serial_number;        // Device serial Number
	sds manufacturer;         // Manufacturer String
	sds product;              // Product string
  sds vendor_name;          // Vendor name from data base
  sds id;                   // unique identification string, containing serialnumber, physical port etc.
	struct hidraw_device_info *next; // Pointer to the next device
};


void print_info(struct hidraw_device_info *dev_info){
  printf("Device info:\n");
  printf("  Vendor: %04X:%04X\n",dev_info->vendor_id, dev_info->product_id);
  printf("  Device node: %s\n",dev_info->device_node);
  printf("  Parent device node: %s\n",dev_info->parent_device_node);
  printf("  Serial number: %s\n",dev_info->serial_number);
  printf("  Manufacturer: %s\n",dev_info->manufacturer);
  printf("  Product: %s\n",dev_info->product);
  printf("  Port name: %s\n",dev_info->port);
  printf("  Vendor name: %s\n",dev_info->vendor_name);
  printf("  Unique ID: %s\n",dev_info->id);
  printf("  next: %p\n",(void*)dev_info->next);
}

void free_hid_device_info_subelements(struct hidraw_device_info * this){
  free(this->device_node);
  free(this->serial_number);
  free(this->manufacturer);
  free(this->product);
  free(this->port);
  free(this->vendor_name );
  free(this->id );
}

void free_enumerate_hid_devices(struct hidraw_device_info * this){
  while( this != NULL){
    struct hidraw_device_info * next = this->next;
    free_hid_device_info_subelements(this);
    free(this);
    this = next;
  }
}  


/*
  Enumerate hidraw devices

  search kernel sysfs for hidraw devices, and retrieve device information, using udev.

  Limit search to the specified device attributes

  return: a linked list of devices, terminated with an empty next entry (next = NULL)

*/ 
struct hidraw_device_info * enumerate_hidraw_devices(
  unsigned int vendor_id, 
  unsigned int product_id, 
  char * port, 
  char * serial_number, 
  char * manufacturer_string
){
  struct udev *udev;
  struct udev_enumerate *enumerate, *enemumerate_hidraw;
  struct hidraw_device_info *dev_info, *dev_info_list, *first_entry = NULL;
  struct udev_list_entry *devices, *entry;
  struct udev_device *parent_dev;
  sds str;

  udev = udev_new();
  if (!udev) {
    fprintf(stderr, "udev_new() FAILURE\n");
    return NULL;
  }

  enumerate = udev_enumerate_new(udev);
  if (!enumerate) {
    fprintf(stderr, "udev_enumerate_new() FAILURE\n");
    return NULL;
  }

  debug("Scanning for devices");
	udev_enumerate_add_match_subsystem(enumerate, "hidraw");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);

  udev_list_entry_foreach(entry, devices) {
    const char * strp;
    struct udev_device *dev, *parent_dev;
    int length,i;
    sds *sds_array;
    int bus_type;
    struct hidraw_device_info device_info;
  
    memset(&device_info,0,sizeof(device_info));
    device_info.sysfs_path = sdsnew(udev_list_entry_get_name(entry));
    debug("udev_list_entry_get_name: %s\n",device_info.sysfs_path);
    sds_array = sdssplitlen(device_info.sysfs_path, sdslen(device_info.sysfs_path), "/", 1, &length);
    // Find hidraw
    for (i = 4; i < length; i++)
      if(!strcmp(sds_array[i],"hidraw")) break;
    debug("  port: %s\n", sdsjoinsds(&sds_array[3], i-4, "/", 1));
    device_info.port = sdsjoinsds(&sds_array[3], i-4, "/", 1);
    sdsfreesplitres(sds_array,length);

    if (!(dev = udev_device_new_from_syspath(udev, device_info.sysfs_path))) {
      perror("ERROR: Unable to access device");
      break;

    } else do {
      if( !( strp = udev_device_get_devnode(dev))) {
        perror("ERROR: Unable to locate device node");
        break;
      }

      // device_info.device_node = sdsnew(strp);
      debug("  Device node: %s\n",strp);

      // HIDAPI use a special path made of bus address: <bus:4>:<port:4>:<serial:2>
      sds_array = sdssplitlen(strp, strlen(strp), "/", 1, &length);
      if( length >=2 ) {
        int address[2];
        for( i = 0; i < length && i <2; i++ )
          address[i] = strtoll(sds_array[i], NULL, 10) &0xFFFF;
        device_info.device_node = sdscatprintf(sdsempty(),"%04x:%04x:00",address[0],address[1]);
      }
      sdsfreesplitres(sds_array,length);
      if( length <2) {
        fprintf(stderr,"ERROR: Bus addres doesn't match expected  format (<bus>:<port>:<serial>: %s)\n", strp);
        break;
      }

      // Get description from hidraw parent device
      if( !(parent_dev = udev_device_get_parent_with_subsystem_devtype(dev,"hid",NULL))) { // Free
        perror("ERROR: Unable to access hidraw parent device");
     
      } else do {
        sds *sds_array, *line;
        int length, i;

        // get bustype and vendor/product id from parent uevent 
        strp = udev_device_get_sysattr_value(parent_dev, "uevent");
        //debug("Uevent = %s\n",strp);
        line = sdssplitlen(strp, strlen(strp), "\n", 1, &length);
        for(i = 0; i < length; i++) {
          int length;
          sds_array = sdssplitlen(line[i],sdslen(line[i]), "=", 1, &length);
          if( length >= 2 ) {
            //debug("  %s: %s\n",sds_array[0], sds_array[1]);
            if( !strcmp(sds_array[0],"HID_ID")) {
              int length;
              sds *id = sdssplitlen(sds_array[1],sdslen(sds_array[1]), ":", 1, &length);
              if( length >=3 ) {
                debug("  ID: %04LX:%04LX\n",strtoll(id[1], NULL, 16),strtoll(id[2], NULL, 16));
                device_info.bus_type = strtoll(id[0], NULL, 16) &0xFFFF;
                device_info.vendor_id = strtoll(id[1], NULL, 16) &0xFFFF;
                device_info.product_id = strtoll(id[2], NULL, 16) &0xFFFF;
                debug("  Bus type: %s\n",bus_type == 3 ? "USB" : id[0]);
              }
              sdsfreesplitres(id,length);
            }
          } else
            break;
          sdsfreesplitres(sds_array,length);
        }
        //udev_device_unref(parent_dev);

        // Get description from USB parent device
        if ( device_info.bus_type == 3 ) { // USB
          if( !(parent_dev = udev_device_get_parent_with_subsystem_devtype(dev,"usb","usb_device"))) {// Free
            perror("ERROR: Unable to access USB parent device");
         
          } else do {
            if( !(strp = udev_device_get_devnode( parent_dev ))){
              perror("ERROR: Unable to locate parent device");
              break;
            }
            debug("  Parent USB device node: %s\n",strp);
            device_info.parent_device_node = sdsnew(strp);
            
    				/* Manufacturer and Product strings */
            strp = udev_device_get_sysattr_value(parent_dev, "manufacturer");
            debug("  manufacturer: %s\n", strp);
            device_info.manufacturer = sdsnew(strp && strp[0] ? strp : "");
            strp = udev_device_get_sysattr_value(parent_dev, "product");
            debug("  product: %s\n", strp);
            device_info.product = sdsnew(strp && strp[0] ? strp : "");
            strp = udev_device_get_sysattr_value(parent_dev, "serial");
            debug("  serial: %s\n",strp);            
            device_info.serial_number = sdsnew(strp && strp[0] ? strp : "");
            strp = udev_device_get_property_value(parent_dev, "ID_VENDOR_FROM_DATABASE");
            debug("  vendor_name: %s\n",strp);
            device_info.vendor_name = sdsnew(strp && strp[0] ? strp : "");

            assert( device_info.vendor_id && device_info.product_id && device_info.port && device_info.serial_number &&  device_info.manufacturer);

            // Create ID string
            device_info.id = sdscatprintf(sdsempty(), "%04X&%04X&%s&%s&%s",
              device_info.vendor_id,
              device_info.product_id,
              device_info.port,
              device_info.serial_number,
              device_info.manufacturer
            ); 
            debug("  ID string: %s\n",device_info.id);
            //udev_device_unref(parent_dev);
          } while ( 0 );
        }

        // Match
        if(  (!vendor_id || vendor_id == device_info.vendor_id )
          && ( !product_id || product_id == device_info.product_id )  
          && ( !serial_number || !serial_number[0] || ( device_info.serial_number && !strcmp(serial_number,device_info.serial_number) ) )
          && ( !port || !port[0] || ( device_info.port && !strcmp(port, device_info.port) ))
          && ( !manufacturer_string || !manufacturer_string[0]|| ( device_info.manufacturer && !strcmp(manufacturer_string,device_info.manufacturer) ) ) 
        ) {
          // Store devide info in list
          dev_info = malloc(sizeof(struct hidraw_device_info));
          memcpy(dev_info, &device_info, sizeof(device_info));
          dev_info->next = NULL;
          if( ! first_entry ) {
            first_entry = dev_info_list = dev_info;
          } else {  
            dev_info_list->next = dev_info;
            dev_info_list = dev_info;
          }
        }  
      } while ( 0 );
      udev_device_unref(dev);
    } while ( 0 );
  }
  udev_enumerate_unref(enumerate);
  udev_unref(udev);

  return first_entry;
}


union HID_FRAME {
  char raw[16];
  struct {
    uint8_t  cmd;          // command READ/WRITE  
    uint8_t  len;          // message length
    uint8_t  byte1;        // relay state read: MSB, write LSB
    uint8_t  byte2;        // relay state read: LSB, write MSB
    uint8_t  reserved[6];  // reserved bytes
    uint8_t  signature[4]; // command signature
    uint8_t  chk_lsb;      // LSB checksum 
    uint8_t  chk_msb;      // MSB checksum 
  } HID_repport ;
};

#define DEBUG 1

static int get_state(int fd)
{
  int i, bit_array;
  union HID_FRAME hid_msg;
  unsigned int checksum=0;
  sds str;
  
  // Create HID repport: Read status request
  memset(&hid_msg.raw, 0x11, sizeof(hid_msg));
  hid_msg.HID_repport.cmd = 0xD2;
  hid_msg.HID_repport.len = sizeof(hid_msg) - 2;
  memcpy(hid_msg.HID_repport.signature, "HIDC", 4);

  // Calculate check sum
  for(i=0; i<hid_msg.HID_repport.len; i++) 
    checksum += hid_msg.raw[i];
  hid_msg.HID_repport.chk_lsb = checksum & 0x00ff;
  hid_msg.HID_repport.chk_msb = (checksum & 0xff00) >> 8;

  str = sdscatprintf(sdsempty(),"Sending HID repport to device (%ld bytes):\n",sizeof(hid_msg)); 
  for (i=0; i<sizeof(hid_msg); i++) 
    str = sdscatprintf(str, "%02X ", hid_msg.raw[i] & 0xff); 
  str = sdscat(str, "\n "); 
  debug("%s",str); 
  sdsfree(str);

  if (write(fd, hid_msg.raw, sizeof(sizeof(hid_msg))) <= 0) {
    perror("Failed to write to HID device");
    return -1;
  }

  // Read response
  memset(hid_msg.raw,0,sizeof(hid_msg));
  if (read(fd, hid_msg.raw, sizeof(hid_msg)) < 0) {
    perror("Failed to read from HID device");
    return -2;
  }
  // Big endian
  bit_array = hid_msg.HID_repport.byte2 + (hid_msg.HID_repport.byte1 << 8);

  debug("Recieved HID repport from device: "); 
    for (i=0; i<sizeof(hid_msg); i++) 
      debug("%02X ", hid_msg.raw[i]); 
    debug("\n"); 
   debug("Relay state = 0x%04x\n", bit_array); 

  return bit_array;
}

int h_open(const char *path)
{
  int fd, use;
  int i, res, desc_size = 0;
	char buf[256];
	struct hidraw_report_descriptor rpt_desc;
	struct hidraw_devinfo info;

	/* OPEN HERE */
  debug("Open device: %s\n",path);
	fd = open(path, O_RDWR | O_NONBLOCK);
	
	/* If we have a good handle, return it. */
	if (fd > 0) {
    memset(&rpt_desc, 0x0, sizeof(rpt_desc));
  	memset(&info, 0x0, sizeof(info));
  	memset(buf, 0x0, sizeof(buf));

  	/* Get Report Descriptor Size */
  	res = ioctl(fd, HIDIOCGRDESCSIZE, &desc_size);
  	if (res < 0)
  		perror("HIDIOCGRDESCSIZE");
  	else
  		printf("Report Descriptor Size: %d\n", desc_size);

  	/* Get Report Descriptor */
  	rpt_desc.size = desc_size;
  	res = ioctl(fd, HIDIOCGRDESC, &rpt_desc);
  	if (res < 0) {
  		perror("HIDIOCGRDESC");
  	} else {
  		printf("Report Descriptor:\n");
  		for (i = 0; i < rpt_desc.size; i++)
  			printf("%hhx ", rpt_desc.value[i]);
  		puts("\n");
  	}

  	/* Get Raw Name */
  	res = ioctl(fd, HIDIOCGRAWNAME(256), buf);
  	if (res < 0)
  		perror("HIDIOCGRAWNAME");
  	else
  		printf("Raw Name: %s\n", buf);

  	/* Get Physical Location */
  	res = ioctl(fd, HIDIOCGRAWPHYS(256), buf);
  	if (res < 0)
  		perror("HIDIOCGRAWPHYS");
  	else
  		printf("Raw Phys: %s\n", buf);

  	/* Get Raw Info */
  	res = ioctl(fd, HIDIOCGRAWINFO, &info);
  	if (res < 0) {
  		perror("HIDIOCGRAWINFO");
  	} else {
  		printf("Raw Info:\n");
  		printf("\tbustype: %d \n",info.bustype);
  		printf("\tvendor: 0x%04hx\n", info.vendor);
  		printf("\tproduct: 0x%04hx\n", info.product);
  	}


#ifdef sadfasd
		/* Get the report descriptor */
		int res, desc_size = 0;
		struct hidraw_report_descriptor rpt_desc;

		memset(&rpt_desc, 0x0, sizeof(rpt_desc));

		/* Get Report Descriptor Size */
		res = ioctl(fd, HIDIOCGRDESCSIZE, &desc_size);
		if (res < 0)
			perror("HIDIOCGRDESCSIZE");


		/* Get Report Descriptor */
		rpt_desc.size = desc_size;
		res = ioctl(fd, HIDIOCGRDESC, &rpt_desc);
		if (res < 0) {
			perror("HIDIOCGRDESC");
		} else {
			/* Determine if this device uses numbered reports. */
			;//use = uses_numbered_reports(rpt_desc.value,rpt_desc.size);

		}
#endif
		return fd;
	}
	else {
		perror("Unable to open devices");
		return -1;
	}
}

void print_hid_dev_info(struct hid_device_info * dev_info){

  printf("Path: %s\n", dev_info->path);
  printf("  vendor_id: %04X\n", dev_info->vendor_id);
  printf("  product_id: %04X\n", dev_info->product_id);
  printf("  serial_number: %ls\n", dev_info->serial_number);
  printf("  release_number: %d\n", dev_info->release_number);
  printf("  manufacturer_string: %ls\n", dev_info->manufacturer_string);
  printf("  product_string: %ls\n", dev_info->product_string);
  printf("  interface_number: %d\n", dev_info->interface_number);
  printf("  Next: %p\n", dev_info->next);
}

union FRAME {
  char raw[16];
  struct {
    uint8_t  cmd;          // command READ/WRITE  
    uint8_t  len;          // message length
    uint8_t  byte1;        // relay state read: MSB, write LSB
    uint8_t  byte2;        // relay state read: LSB, write MSB
    uint8_t  reserved[6];  // reserved bytes
    uint8_t  signature[4]; // command signature
    uint8_t  chk_lsb;      // LSB checksum 
    uint8_t  chk_msb;      // MSB checksum 
  } HID_repport ;
};

static int get_relay_state(hid_device *handle)
{
  int i;
  union FRAME hid_msg;
  unsigned int checksum=0;
  int states = -1;
  
  // Create HID repport read status Request
  memset(&hid_msg.raw, 0x11, sizeof(hid_msg));
  hid_msg.HID_repport.cmd = 0xD2;
  hid_msg.HID_repport.len = sizeof(hid_msg) - 2;
  memcpy(hid_msg.HID_repport.signature, "HIDC", 4);

  // Check sum
  for(i=0; i<hid_msg.HID_repport.len; i++) 
    checksum += hid_msg.raw[i];
  hid_msg.HID_repport.chk_lsb = checksum & 0x00ff;
  hid_msg.HID_repport.chk_msb = (checksum & 0xff00) >> 8;

  #ifdef DEBUG
    printf("Sending HID repport to device (%ld bytes): ",sizeof(hid_msg)); 
    for (i=0; i<sizeof(hid_msg); i++) printf("%.02X ", 0xff & hid_msg.raw[i]); 
     printf("\n"); 
  #endif

  if ( (i = hid_write(handle, hid_msg.raw, sizeof(hid_msg))) <= 0) {
    perror("Failed to write to HID device");
    return FAILURE;
  }
printf("# %d\n",i);

  // Read response
  memset(hid_msg.raw,0,sizeof(hid_msg));
  if (hid_read(handle, hid_msg.raw, sizeof(hid_msg)) < 0) {
    perror("Failed to read from HID device");
    return FAILURE;
  }
  // Big endian
  states = hid_msg.HID_repport.byte2 + (hid_msg.HID_repport.byte1 << 8);

  #ifdef DEBUG
    printf("Recieved HID repport from device: "); 
    for (i=0; i<sizeof(hid_msg); i++) 
      printf("%02X ", hid_msg.raw[i]); 
    printf("\n"); 
   printf("Relay state = 0x%04x\n", states); 
  #endif

  return SUCCESS;
}

int ymain(int argc, char *argv[]){
  struct hidraw_device_info *device_info = NULL;
  struct hidraw_devinfo info;
  int    file_descriptor; 
  int    bit_array;
  uint16_t relay_states, state;

  struct hid_device_info *dev_info;
  struct hid_device_info *dev, *nextdev;
  hid_device *device;
  
  dev = hid_enumerate(0x0416,0x5020);
  while( dev ) {
    print_hid_dev_info(dev);
    device = hid_open_path(dev->path);
    if ( device ) {
      printf("  Reading: %x\n",get_relay_state(device));
      hid_close(device);
    } else {
      fprintf(stderr,"Error: Unable to open device (%s)\n",dev->path);
    }
    dev = dev->next;
  }


  /*
  device_info = enumerate_hidraw_devices(0x0416,0x5020, NULL, NULL, "Nuvoton");
  //dev_info = hid_enumerate(0x0416,0x5020);
 
  while( device_info ){
    print_info(device_info);
    hid_device *dev = hid_open_path(device_info->device_node);
    if( dev ){
      printf("  Reading: %x\n",get_relay_state(dev));
    }
    device_info = device_info->next;
  
  } 
  */

  //free_enumerate_hid_devices(device_info);
  return SUCCESS;
}
