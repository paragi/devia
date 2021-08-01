struct hid_device_info * hidusb_enumerate_match(
  unsigned int vendor_id, 
  unsigned int product_id, 
  char * serial_number, 
  char * manufacturer_string,
  char * path
);
int probe_hidusb(struct _device_identifier id, struct _device_list ** device_list);
