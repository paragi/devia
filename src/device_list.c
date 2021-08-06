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
    "Nuvoton",
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

const struct _supported_device usb_device[] = 
{
  { NULL }
};

const struct _supported_device gpio_device[] = 
{
  { NULL }
};

const struct _supported_device serial_device[] = 
{
  { NULL }
};

const struct _supported_device onewire_device[] = 
{
  {
    "DS18B20",
    "DS18B20 1-Wire temperature sensor with 9 to 12-bit precision, -55C to 125C (+/-0.5C)",
    NULL,
    action_nuvoton
  },
  { NULL }
};

// All
const struct _supported_interface supported_interface[] =
{
  {"dummy", "Internal test devices", probe_dummy, dummy_device},
  {"hidusb", "HID USB devices", probe_hidusb, hidusb_device},
  {"gpio", "General purpose IO chip",NULL, gpio_device},
  {"serial", "Serial (com/tty) devices", NULL, serial_device},
  {"onewire","one-wire (w1) interfaced devices", NULL, onewire_device},
  {NULL}
};

