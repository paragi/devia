#include "sds.h"

#ifndef COMMON_INCL
#define COMMON_INCL

#define SUCCESS 0
#define FAILURE -1

// This goes to common.h
enum actions {
  NC,
  OFF,
  ON,
  TOGGLE,
  NO_ACTION 
};

const char action_names[][32] = {
  "nc",
  "off",
  "on",
  "toggle",
  "no action" 
};

// unique device identifier format: <interface>+<vendor_id>:<product_id>+<serial_number>+<port>+<manufacturer string>
struct _device_identifier {
  sds interface;
  int vendor_id;
  int product_id;
  sds serial_number;
  sds port;
  sds manufacturer_string;

};

// List of devices
struct _device_list {
  void * handle;
  sds id;
  sds group;
  int (* get)( struct _device_identifier *, int);
  int (* set)( struct _device_identifier *, int);
  sds name;
  sds *label;
  struct _device_list *next;
};

#endif
