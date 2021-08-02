#include "sds.h"

#ifndef COMMON_INCL
#define COMMON_INCL

#define SUCCESS 0
#define FAILURE -1

// unique device identifier format: <interface>+<vendor_id>:<product_id>+<serial_number>+<port>+<manufacturer string>
struct _device_identifier {
  sds interface;
  sds device_id;
  int vendor_id;
  int product_id;
  sds manufacturer_string;
  sds serial_number;
  sds port;
  sds device_path;
};

// List of devices
struct _device_list {
  sds name;
  sds id;
  sds path;
  sds group;
  int (* action)( struct _device_list *, sds, sds, sds *);
  struct _device_list *next;
};

extern int info;

#endif
