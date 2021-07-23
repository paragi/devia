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

#include <stdlib.h>
#include <error.h>
#include <argp.h>

#define DEBUG
// #define TEST

#define SUCCESS 0
#define FAILURE -1

// This goes to common.h
enum actions {
  NC,
  OFF,
  ON,
  TOGGLE,
  NO_ACTION 
};

const char action_names[][32] = {
  "nc",
  "off",
  "on",
  "toggle",
  "no action" 
};

// unique device identifier format: <interface>+<vendor_id>:<product_id>+<serial_number>+<port>+<manufacturer string>
struct device_identifier {
  sds interface;
  int vendor_id;
  int product_id;
  sds serial_number;
  sds port;
  sds manufacturer_string;
};

// List of devices
struct device_list {
  void * handle;
  sds id;
  sds group;
  struct device_list *next;
};


/*---------------------------------------------------------------*/
// Local definitions
const char *argp_program_version = "rly 1.0";
const char *argp_program_bug_address = "github.com/paragi/rly/issues";

/* Program documentation. */
static char doc[] =
  "rly -- command line relay control program.\n";

/* A description of the arguments we accept. */
static char args_doc[] = "rly [-l --list] [<unique identifier> [,<relay|all|0 > [,<action (on|off|toggle)>]]]";

/* Keys for options without short-options. */
#define OPT_ABORT  1            /* –abort */

/* The options we understand. */
static struct argp_option options[] = {
  {"list",  'l', 0,       0, "List devices" },
  { 0 }
};
// Used by main to communicate with parse_opt. 
struct arguments {
  int list;       // ‘-l’
  int no_arg;
  struct device_identifier id;
  int relay_id;
  enum actions action;
};

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
  printf("  relay id: %d\n", argument.relay_id);
  printf("  action: %s\n", action_names[argument.action]);

}
// This is where arguments are parsed, one at a time, as they occur
static error_t parse_opt (int key, char *arg, struct argp_state *state) {
  /* Get the input argument from argp_parse, which we know is a pointer to our   structure. */
  struct arguments *argument = state->input;

  switch (key) {
    case 'l': case 's':
      argument->list = 1;
      break;
  
    case ARGP_KEY_ARG:
        /* There are remaining arguments not parsed by any parser, which may be found
       starting at (STATE->argv + STATE->next).  If success is returned, but
       STATE->next left untouched, it's assumed that all arguments were consume,
       otherwise, the parser should adjust STATE->next to reflect any arguments
       consumed.  */
      switch (state->arg_num) {
        case 0: { // split unique device identifier 
          int length, length2;
          sds *sds_array, *sds_array2;
          sds_array = sdssplitlen(arg,strlen(arg), "+", 1, &length);

          /*printf("unique device identifier Array = {\n");
          for (int i = 0; i < length; i++)
            printf("%d %s\n", i, sds_array[i]);
          printf("}\n");
          */
          do {
            if(length < 1) break;
            argument->id.interface = sdsnew(sds_array[0]);
            if(length < 2) break;
            sds_array2 = sdssplitlen(sds_array[1],sdslen(sds_array[1]), ":", 1, &length2);
            if (length2 >0)  
              argument->id.vendor_id =strtol(sds_array2[0],NULL,16);
            if (length2 >1)  
              argument->id.product_id = strtol(sds_array2[1],NULL,16);
            sdsfreesplitres(sds_array2, length2); 
            if(length < 3) break; 
            argument->id.serial_number = sdsnew(sds_array[2]);
            if(length < 4) break; 
            argument->id.port = sdsnew(sds_array[3]);
            if(length < 5) break; 
            argument->id.manufacturer_string = sdsnew(sds_array[4]);
          }while ( 0 );
          sdsfreesplitres(sds_array, length); 
          break;
        }
        
        case 1: // relay numner / id
          if( !strcmp(strtolower(arg),"all") )
            argument->relay_id = 0;
          else
            argument->relay_id = strtol(arg,NULL,10);  
          break;

        case 2: // Action
          for (size_t i = OFF; i <= NO_ACTION; i++) {
            argument->action = i;
            if (!strcmp(strtolower(arg), action_names[i])) 
              break;
          }
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

// XXX -> YYY -> abcd

int probe(struct device_identifier id, struct device_list ** device){
  printf("  probe(%04X:%04X, %s, %s, %s)\n",
    id.vendor_id, 
    id.product_id,
    id.serial_number,
    id.port,
    id.manufacturer_string
  ); 

  *device = malloc(sizeof(struct device_list)); 
  (*device)->id = "Test device 1";
  (*device)->group = "dailout";
  (*device)->next = NULL;
  device = &(*device)->next; 

  *device = malloc(sizeof(struct device_list)); 
  (*device)->id = "Test device 2";
  (*device)->group = "dailout";
  (*device)->next = NULL;
  device = &(*device)->next; 

  return SUCCESS;
}    

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

int main (int argc, char **argv) {
  int i;
  struct arguments argument;
  struct device_list **first_entry, *device_list = NULL;



  memset(&argument,0,sizeof(argument));
  argp_parse (&argp, argc, argv, 0, 0, &argument);
  // print_arguments(argument);

  // Probe and make a list of mathched devices
  first_entry = &device_list;
  if ( !argument.id.interface || !strcmp(argument.id.interface ? : "","hid") )
    probe(argument.id, &device_list);

  // Interact with relay
  if (!argument.list) 
    if ( argument.action )
      ; // Set relay state
    else
      ; // read state of relays

  // List relays
  for( i = 0, device_list = *first_entry; device_list; i++) {
    printf("device ID: %s - group: %s\n",device_list->id, device_list->group);
    device_list = device_list->next;
  }
  if( i < 1 )
    printf("No devices found %d\n",i);


  exit (0);
}