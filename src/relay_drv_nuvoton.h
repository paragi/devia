/******************************************************************************
*
* Relay card control utility: Driver for Nuvoton USB-HID relay controller:
* By: Simon Rigét @ Paragi 2021
*
* This file is part of crelay.
* 
*****************************************************************************/ 
#ifndef relay_drv_nuvoton_h
  #define relay_drv_nuvoton_h

  // Extention of hidapi info structure 
  struct usb_device_info_extendet {
  	char *device_node;            // Platform-specific device device_node
  	unsigned short vendor_id;     // Device Vendor ID
  	unsigned short product_id;    // Device Product ID
  	// wchar_t *serial_number;    // Serial Number
  	char *serial_number;          // Serial Number
  	unsigned short release_number;// Device Release Number in binary-coded decimal, also known as Device Version Number
  	//wchar_t *manufacturer_string; // Manufacturer String
  	char *manufacturer_string; // Manufacturer String
  	//wchar_t *product_string;      // Product string
  	char *product_string;      // Product string
  	// Valid on both Linux implementations in all cases.
  	// Valid on the Windows implementation only if the device
  	// contains more than one interface.
  	// Valid on the Mac implementation if and only if the device is a USB HID device.
  	struct usb_device_info_extendet *next; // Pointer to the next device
    char *port;                   // Add on - unique description of port, the device is attached to 
    char * vendor_name;           // Vendor name from data base
    char *id;                     // unique identification string, containing serialnumber, physical port etc.
  };

  // Public functions
  int detect_relay_controller_nuvoton(char* address, uint8_t* num_relays, char* serial, relay_info_t** relay_info);
  int get_relay_nuvoton(char* address, uint8_t relay, relay_state_t* relay_state, char* serial);
  int set_relay_nuvoton(char* address, uint8_t relay, relay_state_t relay_state, char* serial);
#endif
