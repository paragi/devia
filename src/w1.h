#ifndef W1_H
#define W1_H

/* Application */
#include "toolbox.h"
#include "common.h"

int probe_w1(int si_index, struct _device_identifier id, GList **device_list);
int recognize_w1(int si_index,  void * dev_info );
int action_w1(struct _device_list *device, sds attribute, sds action, sds *reply);

#endif