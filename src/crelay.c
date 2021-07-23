/******************************************************************************
 * 
 * Relay card control utility: Main module
 * 
 * Description:
 *   This software is used to controls different type of relays cards.
 *   There are 3 ways to control the relays:
 *    1. via command line
 *    2. via web interface using a browser
 *    3. via HTTP API using a client application
 * 
 * Author:
 *   Ondrej Wisniewski (ondrej.wisniewski *at* gmail.com)
 *
 * Build instructions:
 *   make
 *   sudo make install
 * 
 * Last modified:
 *   14/01/2019
 *
 * Copyright 2015-2019, Ondrej Wisniewski 
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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "data_types.h"
#include "config.h"
#include "relay_drv.h"

#define VERSION "0.14"
#define DATE "2019"

/* HTTP server defines */
#define SERVER "crelay/"VERSION
#define PROTOCOL "HTTP/1.1"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define API_URL "gpio"
#define DEFAULT_SERVER_PORT 8000

/* HTML tag definitions */
#define RELAY_TAG "pin"
#define STATE_TAG "status"
#define SERIAL_TAG "serial"

#define CONFIG_FILE "/etc/crelay.conf"

/* Global variables */
config_t config;

#ifdef NOT_USED
static char rlabels[MAX_NUM_RELAYS][32] = {"My appliance 1", "My appliance 2", "My appliance 3", "My appliance 4",
                                           "My appliance 5", "My appliance 6", "My appliance 7", "My appliance 8"};                                       



/**********************************************************
 * Function: exit_handler()
 * 
 * Description:
 *           Handles the cleanup at reception of the 
 *           TERM signal.
 * 
 * Returns:  -
 *********************************************************/
static void exit_handler(int signum)
{
   syslog(LOG_DAEMON | LOG_NOTICE, "Exit crelay daemon\n");
   exit(EXIT_SUCCESS);
}

 

/**********************************************************
 * Function print_usage()
 * 
 * Description:
 * 
 * Parameters:
 * 
 *********************************************************/
void print_usage()
{
   relay_type_t rtype;
   char cname[60];
   
   printf("crelay, version %s\n\n", VERSION);
   printf("This utility provides a unified way of controlling different types of relay cards.\n");
   printf("Supported relay cards:\n");
   for(rtype=NO_RELAY_TYPE+1; rtype<LAST_RELAY_TYPE; rtype++)
   {
      crelay_get_relay_card_name(rtype, cname);
      printf("  - %s\n", cname);
   }
   printf("\n");
   printf("The program can be run in interactive (command line) mode or in daemon mode with\n");
   printf("built-in web server.\n\n");
   printf("Interactive mode:\n");
   printf("    crelay -i | [-s <serial number>] <relay number> [ON|OFF]\n\n");
   printf("       -i print relay information\n\n");
   printf("       The state of any relay can be read or it can be changed to a new state.\n");
   printf("       If only the relay number is provided then the current state is returned,\n");
   printf("       otherwise the relays state is set to the new value provided as second parameter.\n");
   printf("       The USB communication port is auto detected. The first compatible device\n");
   printf("       found will be used, unless -s switch and a serial number is passed.\n\n");
   printf("Daemon mode:\n");
   printf("    crelay -d|-D [<relay1_label> [<relay2_label> [<relay3_label> [<relay4_label>]]]] \n\n");
   printf("       -d use daemon mode, run in foreground\n");
   printf("       -D use daemon mode, run in background\n\n");
   printf("       In daemon mode the built-in web server will be started and the relays\n");
   printf("       can be completely controlled via a Web browser GUI or HTTP API.\n");
   printf("       The config file %s will be used, if present.\n", CONFIG_FILE);
   printf("       Optionally a personal label for each relay can be supplied as command\n");
   printf("       line parameter which will be displayed next to the relay name on the\n");
   printf("       web page.\n\n");
   printf("       To access the web interface point your Web browser to the following address:\n");
   printf("       http://<my-ip-address>:%d\n\n", DEFAULT_SERVER_PORT);
   printf("       To use the HTTP API send a POST or GET request from the client to this URL:\n");
   printf("       http://<my-ip-address>:%d/%s\n\n", DEFAULT_SERVER_PORT, API_URL);
}


/**********************************************************
 * Function main()
 * 
 * Description:
 * 
 * Parameters:
 * 
 *********************************************************/
int xmain(int argc, char *argv[])
{
      relay_state_t rstate;
      char com_port[MAX_COM_PORT_NAME_LEN];
      char cname[MAX_RELAY_CARD_NAME_LEN];
      char* serial=NULL;
      uint8_t num_relays=FIRST_RELAY;
      relay_info_t *relay_info;
      int argn = 1;
      int err;
      int i = 1;

   if (argc==1)
   {
      print_usage();
      exit(EXIT_SUCCESS);
   }
   
  /*****  Command line mode *****/

  if (!strcmp(argv[argn],"-i"))
  {
     // Detect all cards connected to the system 
     if (crelay_detect_all_relay_cards(&relay_info) == -1)
     {
        printf("No compatible device detected.\n");
        return -1;
     }
     printf("\nDetected relay cards:\n");
     while (relay_info->next != NULL)
     {
        crelay_get_relay_card_name(relay_info->relay_type, cname);
        printf("  #%d\t%s (serial %s)\n", i++ ,cname, relay_info->serial);
        relay_info = relay_info->next;
        // TODO: free relay_info list memory
     }
     
     exit(EXIT_SUCCESS);
  }

  if (!strcmp(argv[argn], "-s"))
  {
     if (argc > (argn+1))
     {
        serial = malloc(sizeof(char)*strlen(argv[argn+1])+1);
        strcpy(serial, argv[argn+1]);
        argn += 2;
     }
     else
     {
        print_usage();
        exit(EXIT_FAILURE);            
     }
  }

  if (crelay_detect_relay_card(com_port, &num_relays, serial, NULL) == -1)
  {
     printf("** No compatible device detected **\n");
     
     if(geteuid() != 0)
     {
        puts("You might not have permissions to use the wanted device.");
        puts("If the device is connected, check what group the device belongs to.");
        puts("You may find the device group with \"ls -al /dev/<device node name>\"");
        puts("You can add a group to a user with \"usermod -a -G <group name> <user name>\"");
        
        /** Bad very not good pracis from windows, to require root priv 
        printf("\nWarning: this program is currently not running with root privileges !\n");
        printf("Therefore it might not be able to access your relay card communication port.\n");
        printf("Consider invoking the program from the root account or use \"sudo ...\"\n");
        **/
     }
     exit(EXIT_FAILURE);
  }

  switch (argc)
  {
     case 2:
     case 4:
        /* GET current relay state */
        if (crelay_get_relay(com_port, atoi(argv[argn]), &rstate, serial) == 0)
           printf("Relay %d is %s\n", atoi(argv[argn]), (rstate==ON)?"on":"off");
        else
           exit(EXIT_FAILURE);
        break;
        
     case 3:
     case 5:
        /* SET new relay state */
        if (!strcmp(argv[argn+1],"on") || !strcmp(argv[argn+1],"ON"))
           err = crelay_set_relay(com_port, atoi(argv[argn]), ON, serial);
        else if (!strcmp(argv[argn+1],"off") || !strcmp(argv[argn+1],"OFF"))
           err = crelay_set_relay(com_port, atoi(argv[argn]), OFF, serial);
        else 
        {
           print_usage();
           exit(EXIT_FAILURE);
        }
        if (err)
           exit(EXIT_FAILURE);
        break;
        
     default:
        print_usage();
  }
   exit(EXIT_SUCCESS);
}
#endif