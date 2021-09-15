#ifndef SYSFS_H
#define SYSFS_H

/* Linux */
#include <hidapi/hidapi.h>

/* Application */
#include "toolbox.h"
#include "common.h"

int probe_sysfs(int si_index, struct _device_identifier id, GList **device_list);
int recognize_sysfs(int si_index,  void * dev_info );
int action_sysfs(struct _device_list *device, sds attribute, sds action, sds *reply);

#endif