#ifndef HIDUSB_H
#define HIDUSB_H
#include <glib.h>

#include "common.h"

int probe_hidusb(int si_index, struct _device_identifier id, GSList **device_list);

#endif