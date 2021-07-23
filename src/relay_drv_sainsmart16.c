/******************************************************************************
 * 
 * Relay card control utility: Driver for Sainsmart 16-channel USB-HID relay
 * control module:
 * 
 * Note:
 *   We need libhidapi, a multi-platform library which allows an application 
 *   to interface with USB and Bluetooth HID-Class devices:
 *   http://www.signal11.us/oss/hidapi
 *   To install the library and development files on a Debian based system:
 *   apt-get install libhidapi-libusb0 libhidapi-dev
 * 
 * Description:
 *   This 16-channel module is used for USB control of the Sainsmart 16-channel
 *   relays.
 *   http://www.sainsmart.com/sainsmart-16-channel-controller-usb-hid-programmable-control-relay-module.html
 * 
 * Author:
 *   Kevin Hilman (khilman *at* kernel.org)
 *
 *   Based on the Node.js relay project by Michael Vines:
 *   * https://github.com/mvines/relay
 * 
 * Build instructions:
 *   gcc -c relay_drv_hidapi_sain.c
 * 
 * Last modified:
 *   28/03/2016
 *
 * Copyright 2016, Kevin Hilman
 * 
 * This file is part of crelay.
 * 
 * crelay is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with crelay.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *****************************************************************************/ 

/******************************************************************************
 * Communication protocol description
 * ==================================
 * 
 * Read command
 * ------------
 * 
 *   Request (16 bytes)
 * 
 *   Send the following buffer using hid_write():
 *   RD LN 11 11 11 11 11 11 11 11 'H' 'I' 'D' 'C' CS CS
 * 
 *   RD: read command (0xD2)
 *   LN: message length (excluding checksum)
 *   CS: checksum bytes (16 bits)
 * 
 *   Response
 * 
 *   Receive the response using hid_read():
 *   XX XX BM BM
 *   BM: Relay state bitmap (16 bits)
 *   XX: unknown meaning
 * 
 *   Relay State Bitmap:
 *    15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0    bits
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *   | 15| 0 | 14| 1 | 13| 2 | 12| 3 | 11| 4 | 10| 5 | 9 | 6 | 8 | 7 |  relay number
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 
 * Write command
 * -------------
 * 
 *   Request (16 bytes)
 * 
 *   Send the following buffer using hid_write():
 *   WR LN BM BM 00 00 00 00 00 00 'H' 'I' 'D' 'C' CS CS
 * 
 *   WR: write command (0xC3)
 *   LN: message length (excluding checksum)
 *   BM: Relay state bitmap (16 bits)
 *   CS: checksum bytes (16 bits)
 * 
 *   Response:
 *   None
 *   
 *   Relay State Bitmap:
 *    15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0    bits
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *   | 15| 14| 13| 12| 11| 10| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |  relay number
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

 *****************************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <hidapi/hidapi.h>

#include "relay_drv.h"

#define VENDOR_ID 0x0416
#define DEVICE_ID 0x5020

#define CMD_READ  0xD2
#define CMD_WRITE 0xC3
#define CMD_SIGNATURE "HIDC"


/* USB HID message structure */
typedef struct
{
   uint8_t  cmd;          // command READ/WRITE  
   uint8_t  len;          // message length
   uint16_t bitmap;       // relay state bitmap
   uint8_t  reserved[6];  // reserved bytes
   uint8_t  signature[4]; // command signature
   uint16_t chksum;       // 16 bit checksum 
} hid_msg_t;

/* Association between relay number (array index) and bit position */
static uint8_t relay_bit_pos[] = {7 , 8 , 6 , 9 , 5 , 10, 4 , 11, 3 , 12, 2 , 13, 1 , 14, 0 , 15};

static uint8_t g_num_relays=SAINSMART16_USB_NUM_RELAYS;


static void init_hid_msg(hid_msg_t *hid_msg, uint8_t cmd, uint16_t bitmap)
{
   int i;
   uint16_t checksum=0;

   if (cmd==CMD_READ)
     memset(hid_msg, 0x11, sizeof(hid_msg_t));
   else
     memset(hid_msg, 0x00, sizeof(hid_msg_t));

   hid_msg->cmd = cmd;
   hid_msg->len = sizeof(hid_msg_t) - 2;
   hid_msg->bitmap = bitmap;
   memcpy(hid_msg->signature, CMD_SIGNATURE, 4);
   for (i=0; i<hid_msg->len; i++) checksum += *(((uint8_t*)hid_msg)+i);
   hid_msg->chksum = checksum;
   
   /* printf("DBG: msg "); */
   /* for (i=0; i<sizeof(hid_msg_t); i++) printf("%02X ", *(((uint8_t*)hid_msg)+i)); */
   /* printf("\n"); */
}


static int get_mask(hid_device *handle, uint16_t *bitmap)
{
  int i;
  hid_msg_t  hid_msg;
  uint16_t mask;
  
  init_hid_msg(&hid_msg, CMD_READ, 0x1111);

  if (hid_write(handle, (unsigned char *)&hid_msg, sizeof(hid_msg)) < 0)
  {
    return -1;
  }
  usleep(1000);
  
  if (hid_read(handle, (unsigned char *)&hid_msg, sizeof(hid_msg)) < 0)
  {
    return -2;
  }
  
  mask = 0;
  for ( i = 0 ; i < g_num_relays; i ++) {
    if (hid_msg.bitmap & (1 << relay_bit_pos[i])) {
      mask |= 1 << i;
    }
  }

  /* printf("DBG: get_mask = 0x%04x\n", hid_msg.bitmap); */
  *bitmap = mask;

  return 0;
}


static int set_mask(hid_device *handle, uint16_t bitmap) 
{
  hid_msg_t  hid_msg;

  /* printf("DBG: set_mask = 0x%04x\n", bitmap); */
  init_hid_msg(&hid_msg, CMD_WRITE, bitmap);
  if (hid_write(handle, (unsigned char *)&hid_msg, sizeof(hid_msg)) < 0)
  {
    return -1;
  }
  return 0;
}


/**********************************************************
 * Function detect_relay_card_sainsmart_16chan()
 * 
 * Description: Detect the Saintsmart 16 channel relay card
 * 
 * Parameters: portname (out) - pointer to a string where
 *                              the detected com port will
 *                              be stored
 *             num_relays(out)- pointer to number of relays
 * 
 * Return:  0 - success
 *         -1 - fail, no relay card found
 *********************************************************/
int detect_relay_card_sainsmart_16chan(char* portname, uint8_t* num_relays, char* serial, relay_info_t** relay_info)
{
   struct hid_device_info *devs, *nextdev;
   uint8_t found=0;
   relay_info_t* rinfo;
   
   if ((devs = hid_enumerate(VENDOR_ID, DEVICE_ID)) == NULL)
   {
      return -1;  
   }

   if (devs->product_string == NULL ||
       devs->path == NULL)
   {
      return -1;
   }
   
   /* Find controller with matching serial number if it's specified */
   nextdev = devs;
   while (nextdev)
   {
      if (relay_info != NULL)
      {
         // Save serial number and type in current relay info struct
         (*relay_info)->relay_type = SAINSMART16_USB_RELAY_TYPE;
         strcpy((*relay_info)->serial, nextdev->path);
         // Allocate new struct
         rinfo = malloc(sizeof(relay_info_t));
         rinfo->next = NULL;
         // Link current to new struct
         (*relay_info)->next = rinfo;
         // Move pointer to new struct
         *relay_info = rinfo;
      }
      else if (serial == NULL || !strcmp(serial, nextdev->path))
      {
         found = 1;
         break;
      }
      nextdev = nextdev->next;
   }

   if (found == 0)
   {
      return -5;
   }

   //printf("DBG: card %ls found\n", devs->product_string);

   /* Return parameters */
   if (num_relays!=NULL) *num_relays = g_num_relays;
   if (portname != NULL) sprintf(portname, "%s", nextdev->path);

   /* printf("DBG: card %ls found, portname=%s\n", devs->product_string, portname); */

   hid_free_enumeration(devs);   
   return 0;
}


/**********************************************************
 * Function get_relay_sainsmart_16chan()
 * 
 * Description: Get the current relay state
 * 
 * Parameters: portname (in)     - communication port
 *             relay (in)        - relay number
 *             relay_state (out) - current relay state
 * 
 * Return:   0 - success
 *          <0 - fail
 *********************************************************/
int get_relay_sainsmart_16chan(char* portname, uint8_t relay, relay_state_t* relay_state, char* serial)
{
   hid_device *hid_dev;
   uint16_t bitmap, bit;
   
   if (relay<FIRST_RELAY || relay>(FIRST_RELAY+g_num_relays-1))
   {  
      fprintf(stderr, "ERROR: Relay number out of range\n");
      return -1;      
   }
   
   /* Open HID API device */
   if ((hid_dev = hid_open_path(portname)) == NULL)
   {
      fprintf(stderr, "unable to open HID API device %s\n", portname);
      return -2;
   }
   
   /* Read relay states */
   if (get_mask(hid_dev, &bitmap) < 0)
   {
      fprintf(stderr, "unable to read data from device %s (%ls)\n", portname, hid_error(hid_dev));
      return -3;
   }
   
   bit = 1 << (relay-1);
   if (bitmap & bit)
     *relay_state = ON;
   else
     *relay_state = OFF;

   /* printf("DBG: get: portname=%s, relay=%d, state=%d\n", portname, relay, (int)*relay_state); */
   hid_close(hid_dev);
   return 0;
}


/**********************************************************
 * Function set_relay_sainsmart_16chan()
 * 
 * Description: Set new relay state
 * 
 * Parameters: portname (in)     - communication port
 *             relay (in)        - relay number
 *             relay_state (in)  - current relay state
 * 
 * Return:   0 - success
 *          <0 - fail
 *********************************************************/
int set_relay_sainsmart_16chan(char* portname, uint8_t relay, relay_state_t relay_state, char* serial)
{ 
   hid_device *hid_dev;
   uint16_t     bitmap;
   
   if (relay<FIRST_RELAY || relay>(FIRST_RELAY+g_num_relays-1))
   {  
      fprintf(stderr, "ERROR: Relay number out of range\n");
      return -1;      
   }
   
   /* Open HID API device */
   if ((hid_dev = hid_open_path(portname)) == NULL)
   {
      fprintf(stderr, "unable to open HID API device %s\n", portname);
      return -2;
   }

   /*
   printf("DBG: Sain16 USB: portname=%s, relay=%d, state=%s\n",
          portname, relay, relay_state == ON? "ON" : "OFF");
   */
   /* Read relay states */
   if (get_mask(hid_dev, &bitmap) < 0)
   {
      fprintf(stderr, "unable to read data from device %s (%ls)\n", portname, hid_error(hid_dev));
      return -3;
   }
   
   /* Set the new relay state bit */
   if (relay_state == OFF) 
   {
      /* Clear the relay bit in mask */
      bitmap &= ~(1<<(relay-1));
   }
   else
   {
      /* Set the relay bit in mask */
      bitmap |= (1<<(relay-1));
   }
   
   /* Write relay states */
   if (set_mask(hid_dev, bitmap) < 0)
   {
      fprintf(stderr, "unable to write data to device %s (%ls)\n", portname, hid_error(hid_dev));
      return -4;
   }
  
   hid_close(hid_dev);
   return 0;
}
