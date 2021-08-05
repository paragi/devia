#ifndef RELAY_NUVOTON
#define RELAY_NUVOTON

/* Linux */
#include <hidapi/hidapi.h>

/* Application */
#include "toolbox.h"
#include "common.h"

int recognize_nuvoton(int si_index,  void * dev_info );
int action_nuvoton(struct _device_list *device, sds attribute, sds action, sds *reply);
#endif