/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <malloc.h>
#include <error.h>
#include <stdbool.h>

/* Unix */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <poll.h>

/* Linux */
#include <hidapi/hidapi.h>
#include <argp.h>
#include <glib.h>
    
/* Application */
#include "toolbox.h"
#include "common.h"
#include "version.h"

#define DEBUG

int info = false; // If set, output information usefull to tinkeres and  developers. 
 
const char *argp_program_version = VERSION_LONG;
const char *argp_program_bug_address = "github.com/paragi/devia/issues.\nDon't hesitate to write a bug repport or feature request ect";

/* Program documentation. */
static char doc[] =
 "devia  [<options>] [<identifier> [<attribute of device> [<action>]]]\n"
  "\n"
  "devia (Device interact) Interacts with one or more attached devices.\n"
  "\n"
  "  <identifyer>: Is a device specific concatanated key, used to identify the  \n"
  "            device. Is is a key consisting of \n"
  "            <interface>&<device identifier>&<port>&<device path> \n"
  "            Each part is separated with a '&'. and can be empty or the end \n"
  "            ommittet.\n"
  "       <interface>: Is the type of interfaces used for the device. ex. usb,  \n"
  "            gpio, serial, hidusb\n"
  "       <Device identifier>: is specific to the interface type. ex: hidusb:\n"
  "            <vendor id>:<product id>:<serial number>:<manufacturer string>\n"
  "       <port>: Is a string that describe the port/bus, the device is \n"
  "            attached to - as the kernel sees it. (sysfs)\n"
  "       <device path>: Is the path to the device as a kernal file.\n"
  "\n"    
  "  <Attribute>: is device specific. ex. a relay number, address or other \n"
  "            attrubute of the device.\n"
  "  <action>: Is device specific. it describes what is to be done to it. Ex: on\n" 
  "            off, toggle, or other value\n"

  "\n"
  "  Note: \n"
  "    - If the <identifier> is ambigious, truncated or missing, it is treated \n"
  "      as a wildcard, and will apply actions to all matching devices.\n"
  "    - if <attribute> (and <action>)is omitted, it is interpreted as a general\n"
  "      status request.\n"
  "    - if <action> is omitted, it is interpretted as a read request, for the\n"
  "      specified attrubute.\n" 
  "    - If a device is interacted with, it is claimed (if posible). Other \n"
  "      process claims, are abandoned.\n"
  "    - Don't use root priviliges, unless for testing porpuses. It is a \n"
  "      serious security risc. (see documenation on how to setup)\n"
  "\n"
  "  Examples:\n"
  "    Set relay 3 = ON, on a Nuvoton HID USB relay controler:\n"
  "       devia hidusb&0416:5020::Nuvoton&& 3 on\n"     
  "\n"
  "    Read state of input pin 4 on GPIO:\n"
  "       devia gpio&pin4 \n"
  "\n"
  "Documentation on https://github.com/paragi/devia.\n"
  "\n"
  ;

/* A description of the arguments */
static char args_doc[] = "[<identifier> [<attribute of device> [<action>]]]";

  /* Keys for options without short-options. */
#define OPT_ABORT  1            /* –abort */

/* The options*/
static struct argp_option options[] = {
  // {NAME, KEY, ARG, FLAGS, DOC} see: https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
  {"list",      'l', 0, 0, "List devices" },
  {"info",      'i', 0, 0, "info readout"},
  {"supported", 's', 0, 0, "List supported devices"},
  { 0 }
};

// Used by main to communicate with parse_opt. 
struct arguments {
  int list;       // -l
  int info;    // -i
  int list_supported_devices; // -s
  int no_arg;
  struct _device_identifier id;
  char * attribute;
  char * action;
};

void print_arguments(struct arguments argument) {
  printf("Argument interpretation:\n");
  printf("  list devices:           %s\n", argument.list ? "true" : "false");
  printf("  list supported devices: %s\n", argument.list_supported_devices ? "true" : "false");
  printf("  Show extra info:        %s\n", argument.info ? "true" : "false");
  printf("  no arguments:           %s\n", argument.no_arg ? "true" : "false");
  printf("  device identifier:\n");
  printf("     interface:   %s\n", argument.id.interface); 
  printf("     device id:   %s\n", argument.id.device_id); 
  printf("     port:        %s\n", argument.id.port); 
  printf("     device path: %s\n", argument.id.device_path); 
  printf("  Attribute:              %s\n", argument.attribute);
  printf("  action:                 %s\n", argument.action);
}

// Parse arguments and options. Arguments are parsed, one at a time, as they occur
static error_t parse_opt (int key, char *arg, struct argp_state *state) {
  /* Get the input argument from argp_parse, which we know is a pointer to our structure. */
  struct arguments *argument = state->input;

  switch (key) {
    case 'l': 
      argument->list = true;
      break;
     case 'i':
      info = argument->info = true;
      break;
    case 's':
      argument->list_supported_devices = true;
      break;
    case ARGP_KEY_ARG:
      /* There are remaining arguments not parsed by any parser, which may be found
      starting at (STATE->argv + STATE->next).  If success is returned, but
      STATE->next left untouched, it's assumed that all arguments were consumed,
      otherwise, the parser should adjust STATE->next to reflect any arguments
      consumed.  */
      switch (state->arg_num) {
        case 0: { // split unique device identifier 
          int length;
          sds *sds_array;
          
          sds_array = sdssplitlen(arg,strlen(arg), "#", 1, &length);
          /*
          printf("unique device identifier Array = {\n");
          for (int i = 0; i < length; i++)
            printf("%d %s\n", i, sds_array[i]);
          printf("}\n");
          */
          do {
            if(length < 1) break;
            argument->id.interface = sdsnew(sds_array[0]);
            if(length < 2) break;
            argument->id.device_id = sdsnew(sds_array[1]);
            if(length < 3) break; 
            argument->id.port = sdsnew(sds_array[2]);            
            if(length < 4) break; 
            argument->id.device_path = sdsnew(sds_array[3]);
          }while ( 0 );
          sdsfreesplitres(sds_array, length); 
          break;
        }
        
        case 1: // Attribute to operate 
          argument->attribute = arg;
          strtolower(argument->attribute);
          break;

        case 2: // Action
          argument->action = arg;
          strtolower(argument->action);
          break;
      }  
      break;

    case ARGP_KEY_ARGS:
      break;  

    /* There are no more command line arguments at all.  */
    case ARGP_KEY_END:
      if ( argument->no_arg && !argument->list && !argument->list_supported_devices) 
        argp_usage(state); // exit
      break;
      /* Because it's common to want to do some special processing if there aren't
         any non-option args, user parsers are called with this key if they didn't
         successfully process any non-option arguments.  Called just before
         ARGP_KEY_END (where more general validity checks on previously parsed
         arguments can take place).  */
    case ARGP_KEY_NO_ARGS:
      /* Passed in before any parsing is done.  Afterwards, the values of each
         element of the CHILD_INPUT field, if any, in the state structure is
         copied to each child's state to be the initial value of the INPUT field.  */
      //argp_usage (state);
      argument->no_arg = true;
      break;
    case ARGP_KEY_INIT:
      /* Use after all other keys, including SUCCESS & END.  */
    case ARGP_KEY_FINI:
      /* Passed in when parsing has successfully been completed (even if there are
         still arguments remaining).  */
    case ARGP_KEY_SUCCESS:
      break;
      /* Passed in if an error occurs.  */
    case ARGP_KEY_ERROR:
    default:
      printf("argp key = %X\n",key);
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

//#define TEST
#ifndef TEST
int main (int argc, char **argv) {
  int i;
  struct arguments argument;
  struct _device_list *entry;
  GSList *device_list = NULL, *iterator = NULL;

  // Parse arguments
  memset(&argument,0,sizeof(argument));
  argp_parse (&argp, argc, argv, 0, 0, &argument);
  
  if ( info ) 
    print_arguments(argument);
  
  // loop through supported devices
  // probe devices and make a list of actual matching devices.
  for( i = 0; supported_interface[i].name; i++) {
    
    // List supported interface and devices
    if( argument.list_supported_devices ) {
      printf("%s:\n", supported_interface[i].description);
      for(int ii = 0; supported_interface[i].device[ii].name; ii++)
        printf("  %s - %s\n",supported_interface[i].device[ii].name, supported_interface[i].device[ii].description);

    // Skip unwanted interfaces  
    } else if ( argument.id.interface 
          && strcmp(argument.id.interface, supported_interface[i].name) ) {
        continue;    

    // Probe and make a list of mathched devices
    } else { 
      if ( info )
        printf("Probing %s\n",  supported_interface[i].name);
      if ( supported_interface[i].probe )
        supported_interface[i].probe(i,argument.id, &device_list);
    }  
  }

  if( argument.list_supported_devices ) 
    exit(0);
  
  if ( info && argument.list ) 
      puts("----------------------------------------------------------------------");
  
  if ( !g_slist_length(device_list) )
    puts("No devices found");

  else for (iterator = device_list; iterator; iterator = iterator->next) {
    entry = (struct _device_list *)iterator->data;
    assert(entry->name);
    assert(entry->id);
    assert(entry->path);
    assert(entry->group);

    // List device and group owner (to discourage use of root privilliges)  
    if (argument.list) {
      printf("%s  id: %s  path: %s  group: %s\n", 
        entry->name, 
        entry->id, 
        entry->path, 
        entry->group
      );

    // Interact with matched devices
    } else {
      sds reply = sdsempty();
      entry->action(entry, argument.attribute, argument.action, &reply);
      printf("%s\n",reply[0] ? reply : "No reply");
    }
  }
  g_slist_free(device_list);
  exit (0);
}
#endif