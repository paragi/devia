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

/* Application */
#include "toolbox.h"
#include "common.h"
#include "hidusb.h"

#define DEBUG
// #define TEST

const char *argp_program_version = "rly 1.0";
const char *argp_program_bug_address = "github.com/paragi/rly/issues";
int verbose = false;

/* Program documentation. */
static char doc[] =
  "rly -- command line relay control program.\n";

/* A description of the arguments we accept. */
static char args_doc[] = "rly [-l --list] [<unique identifier> [,<attribute|all|0 > [,<action (on|off|toggle)>]]]\n\n"\
  "rly controlles one or more devices, specified by the identifier, by performing an action to an attribute, specific to the device.\n"\
  "or rly can list devices available (with the -l option)\n\n"
  "  Unique identifyer: <device type>&<device identifier>&<port>&<device path>\n"\
  "    Device type: supported types are hidusb|gpio|usb|tty\n"\
  "    Device identifier: hidusb: <vendor id>:<product id>: <serial number>,manufacturer string>\n"\
  "                       gpio:   pin<n>\n"\
  //"                       usb:    <vendor id>:<product id>: <serial number>,manufacturer string>\n"
  //"                       tty:    <speed>:<bits>:<stop bits>\n"
  "    port: device specific\n"\
  "    device path:  path to device file. ex: /dev/tty2\n"\
  "  Attribute: typically a number or a name of a device function to interact with\n"\
  "  action: something to do to the attribute ex: on|off\n\n"
  "example: rly hidusb&0416:5020::Nuvoton&& 3 on"
  ;

  /* Keys for options without short-options. */
#define OPT_ABORT  1            /* –abort */

/* The options we understand. */
static struct argp_option options[] = {
  {"list",    'l', 0, 0, "List devices" },
  {"verbose", 'v',0,0,"Verbose readout"},
  { 0 }
};
// Used by main to communicate with parse_opt. 
struct arguments {
  int list;       // -l
  int verbose;    // -v
  int no_arg;
  struct _device_identifier id;
  char * attribute;
  char * action;
};

/*
  Parse arguments and options
  
  This is where arguments are parsed, one at a time, as they occur
*/
static error_t parse_opt (int key, char *arg, struct argp_state *state) {
  /* Get the input argument from argp_parse, which we know is a pointer to our   structure. */
  struct arguments *argument = state->input;

  switch (key) {
    case 'l': 
      argument->list = 1;
      break;
    case 'v': case 'd':
      verbose = argument->verbose = true;
      break;
    case ARGP_KEY_ARG:
      /* There are remaining arguments not parsed by any parser, which may be found
      starting at (STATE->argv + STATE->next).  If success is returned, but
      STATE->next left untouched, it's assumed that all arguments were consume,
      otherwise, the parser should adjust STATE->next to reflect any arguments
      consumed.  */
      switch (state->arg_num) {
        case 0: { // split unique device identifier 
          int length;
          sds * sds_array = sdssplitlen(arg,strlen(arg), "+", 1, &length);

          /*printf("unique device identifier Array = {\n");
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
      // argp_usage (state);
      argument->no_arg = true;
      if( !argument->list ) {
        argument->list = true;
        printf("%s\nUse option -l to avoid this message\n", args_doc);
      }
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

void print_arguments(struct arguments argument) {
  printf("Argument interpretation:\n");
  printf("  no arguments = %s\n", argument.no_arg ? "true" : "false");
  printf("  list devices: %s\n", argument.list ? "true" : "false");
  printf("  device identifier:\n");
  printf("     interface:           %s\n", argument.id.interface); 
  printf("     vendor id:           %04X:%04X\n", argument.id.vendor_id, argument.id.product_id); 
  printf("     serial number:       %s\n", argument.id.serial_number); 
  printf("     port:                %s\n", argument.id.port); 
  printf("     manufacturer string: %s\n", argument.id.manufacturer_string); 
  printf("  Attribute: %s\n", argument.attribute);
  printf("  action: %s\n", argument.action);

}

int probe_dummy(struct _device_identifier id, struct _device_list ** device){

  if ( verbose )
    printf("Probing dummy devices (%s)\n", id.device_id ? : "empty" );
  
  if ( verbose ) printf("  Found device ");
  *device = malloc(sizeof(struct _device_list)); 
  (*device)->id = "Dummy device 1";
  (*device)->group = "dailout";
  (*device)->name = "Dummy device";

  (*device)->next = NULL;
  printf("-- Recognized as %s\n",(*device)->name);
  device = &(*device)->next; 

  if ( verbose ) printf("  Found device ");
  *device = malloc(sizeof(struct _device_list)); 
  (*device)->id = "Dummy device 2";
  (*device)->group = "dailout";
  (*device)->name = "Dummy device";

  (*device)->next = NULL;
  printf("-- Recognized as %s\n",(*device)->name);
  device = &(*device)->next; 

  return SUCCESS;
}    

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

int main (int argc, char **argv) {
  int i;
  struct arguments argument;
  struct _device_list **first_entry, *device_list = NULL;

  // Parse arguments
  memset(&argument,0,sizeof(argument));
  argp_parse (&argp, argc, argv, 0, 0, &argument);
  // print_arguments(argument);

  // Probe and make a list of mathched devices
  first_entry = &device_list;
  if ( !argument.id.interface || !strcmp(argument.id.interface ? : "","dummy") )
    probe_dummy(argument.id, &device_list);

  if ( !argument.id.interface || !strcmp(argument.id.interface ? : "","hidusb") )
    probe_hidusb(argument.id, &device_list);

  // List relays
  if ( verbose ) puts("----------------------------------------------------------------------");
  for( i = 0, device_list = *first_entry; device_list; i++) {
    // Interact with relay
    if (!argument.list) {
      if ( argument.action )
        ; // Set relay state
      else
        ; // read state of relays
    }

    printf("device ID: %s - group: %s\n", device_list->id, device_list->group);
    device_list = device_list->next;
  }
  if( i < 1 )
    printf("No devices found %d\n",i);


  exit (0);
}