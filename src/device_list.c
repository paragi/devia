/* C */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// Application
#include "common.h"

// Device headers
#include "dummy_device.h"
#include "relay_nuvoton.h"
#include "hidusb.h"
#include "sysfs.h"
#include "w1.h"

// Dummy devices
const struct _supported_device dummy_device[] = 
{ 
  {
    "Dummy",
    "Dummy device for internal test purposes",
    recognize_dummy,
    action_dummy
  },
  { NULL }
};
 
//HID USB devices
const struct _supported_device hidusb_device[] = 
{
  {
    "Nuvoton relay controler",
    "USB HID Relay controller 8-16 channels. Nuvoton/Winbond Electronics Corp",
    recognize_nuvoton,
    action_nuvoton
  },
  {
    "SaintSmart",
    "USB HID Relay controller 8-16 channels. Sant smart devices",
    NULL,
    NULL
  },
  {
    "Not Nuvoton",
    "USB HID Rsomthing else",
    NULL,
    NULL
  },
  { NULL }
};

const struct _supported_device sysfs_device[] = 
{
   {
    "SysFS",
    "System kernel file system enabled device",
    NULL,
    action_sysfs
  },
  { NULL }
};

const struct _supported_device onewire_device[] = 
{
  {
    "DS18B20",
    "DS18B20 1-Wire temperature sensor with 9 to 12-bit precision, -55C to 125C (+/-0.5C)",
    NULL,
    action_w1
  },
  { NULL }
};

const struct _supported_device serial_device[] = 
{
  { NULL }
};



const struct _supported_device usb_device[] = 
{
  { NULL }
};
// All
const struct _supported_interface supported_interface[] =
{
  {"dummy", "Internal test devices", probe_dummy, dummy_device},
  {"hidusb", "HID USB devices", probe_hidusb, hidusb_device},
  {"sysfs", "System kernel file system access",probe_sysfs, sysfs_device},
  {"serial", "Serial (com/tty) devices", NULL, serial_device},
  {"w1","one-wire interfaced devices", probe_w1, onewire_device},
  {NULL}
};

