#ifndef HIDUSB_H
#define HIDUSB_H

#include "common.h"

int probe_hidusb(int si_index, struct _device_identifier id, GList **device_list);

#endif  