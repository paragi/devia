

#ifndef COMMON_INCL
#define COMMON_INCL

/* Linux */
#include <hidapi/hidapi.h>

/* Application */
#include "toolbox.h"
#include "hidusb.h"

#define SUCCESS 0
#define FAILURE -1

// unique device identifier format: <interface>+<vendor_id>:<product_id>+<serial_number>+<port>+<manufacturer string>
struct _device_identifier {
  sds interface;
  sds device_id;
  sds port;
  sds device_path;
};

// List of active devices
struct _device_list {
  sds name;
  sds id;
  sds path;
  sds group;
  int (* action)( struct _device_list *, sds, sds, sds *);
  struct _device_list *next;
};

extern int info;


// Supported interfaces and device list
struct _supported_device {
  const char * name;
  const char * description;
  int (*recognize)(int sdl_index, void *dev_info );
  int (*action)(struct _device_list *device, sds attribute, sds action, sds *reply);
};

struct _supported_interface {
  const char *name;
  const char *description;
  int (*probe)(int si_index, struct _device_identifier id, GSList **device_list);
  const struct _supported_device *device;
};

extern const struct _supported_interface supported_interface[];

#endif
