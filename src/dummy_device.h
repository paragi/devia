#ifndef DUMMY_DEVICE
#define DUMMY_DEVICE

/* Linux */
#include <hidapi/hidapi.h>
#include <glib.h>

/* Application */
#include "toolbox.h"
#include "common.h"

int probe_dummy(int si_index, struct _device_identifier id, GList **device_list);

int recognize_dummy(int sdl_index, void * dev_info );
int action_dummy(struct _device_list *device, sds attribute, sds action, sds *reply);

#endif