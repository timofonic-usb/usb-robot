#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>

#include "libusb.h"
#include "output.h"
#include "control.h"

#include "config.h"

// OK, OK, I hate global vars too, but they *are* easier than alternatives in C ;-)
char const* program_name;

static int idVendor=-1;
static int idProduct=-1;

typedef void (*action_t)(void*value);

typedef struct 
{
  char const* string;
  char const* explanation;
  enum 
  {
    none,
    string,
    integer
  }
  value_type;


  action_t action;
}
option
;

static
const char* value_type_string_table[] = 
{
  "","string","integer"
}
;

static
void
option_match( const option* opt, char* argument )
{
  char* pointer;
  long temp;

  if ( strncasecmp( opt->string, argument, strlen(opt->string) ) )
    return;

  
  switch( opt->value_type )
    {
    case none:
      pointer=0;
      break;
    case string:
      pointer = (void*)(argument+strlen(opt->string) );
      break;
    case integer:
      temp = strtol( argument+strlen(opt->string), 0 , 0);
      pointer= (void*)&temp;
      break;
    default:
      cant_get_here();
    }
  opt->action( pointer );
}

static
void
action_idvendor( void*integer )
{
  int* value = (int*)integer;
  idVendor=*value;
}

static
void
action_idproduct( void*integer )
{
  int* value = (int*)integer;
  idProduct=*value;
}

static
void
action_print_version(void*unused)
{
  announce(PACKAGE " version " VERSION);
}

static
void
print_help_and_exit()
     ;

     static
     void
action_print_help_and_exit( void*unused )
{
  print_help_and_exit();
}     

option command_line_opts[]=
{
  {
    "vendor=","set the USB idVendor to look for",integer,action_idvendor 
  },
  {
    "product=","set the USB idProduct to look for",integer,action_idproduct
  },
  {
    "--help","print information about command line arguments",none,action_print_help_and_exit
  },
  {
    "--version","print package and version",none,action_print_version
  },
  {
    0,0,0,0
  }
}
;

static
void
print_help_and_exit()
{
  option* opt;
  
  message( "The purpose of this program is to provide a way to\n"
	   "talk to USB devices from userspace without having to\n"
	   "write and compile a C program specifically for the job\n\n"
	   );

  message( "Command line options:\n" );
  for(opt = command_line_opts;opt->string;opt++ )
    {
      message( "\t%s%s: %s", opt->string,
	       value_type_string_table[ opt->value_type ],
	       opt->explanation
	       );
    }
  exit(EXIT_SUCCESS);
}



usb_dev_handle *
open_device( struct usb_device *device )
{
  usb_dev_handle *handle;

  message( "opening device %s on bus %s",
	   device->filename,
	   device->bus->dirname
	   );

  handle = usb_open(device);
  
  if ( !handle )
    {
      message( "open failed" );
      return 0;
    }

  return handle;
}

int
scan_bus( struct usb_bus* bus )
{
  struct usb_device* roottree = bus->devices;
  struct usb_device *device;


  message( "scanning bus %s", bus->dirname );

  
  for( device = roottree;device;device=device->next)
    {
      if ( (idVendor  ==-1 ? 1 : (device->descriptor.idVendor == idVendor ) ) &&
	   (idProduct ==-1 ? 1 : (device->descriptor.idProduct == idProduct ) ) )
	{
	  usb_dev_handle *device_handle;
	  message( "found device %s on bus %s (idVendor 0x%x idProduct 0x%x)",
		   device->filename,
		   device->bus->dirname,
		   device->descriptor.idVendor,
		   device->descriptor.idProduct );

	  if ( (device_handle = open_device(device)) )
	    {
	      if ( control_device( device_handle ) )
		return 0;
	    }

	  message( "continuing to scan bus %s", bus->dirname );
	}
      else
	message( "device %s on bus %s does not match",
		 device->filename,
		 device->bus->dirname);
    }
  
  return 1;
}

static void print_blurb(void)
{
  announce( "starting " PACKAGE " version " VERSION );
  message( "  (c) 2000, 2001 John Fremlin");
  message( "  Licensed under the GNU Public License version 2, see file COPYING.");
  message( "  You didn't pay me for this program. You have no rights.");
}

int
main(int argc, char** argv)
{
  char**argument;
  struct usb_bus* bus;
  option* opt;
  
  program_name = argv[0];

  print_blurb();

  for(argument=(argv+1);*argument;argument++)
    {
      for(opt = command_line_opts;opt->string;opt++ )
	{
	  option_match(opt,*argument);
	}
    }

  usb_init();
  usb_find_busses();
  usb_find_devices();

  message( "doing bus scan for:" );
  
  if ( idVendor != -1 )
    message( "\tidVendor 0x%x", idVendor );
  else
    message( "\tany idVendor" );
  
  if ( idProduct != -1 )
    message( "\tidProduct 0x%x", idProduct  );
  else
    message( "\tany idProduct" );

  for( bus = usb_busses; bus; bus = bus->next )
    {
      message( "found bus %s", bus->dirname );

      if ( !scan_bus(bus) ) // found device
	{
	  announce( "exiting happily" );
	  exit(EXIT_SUCCESS);
	}
    }
  
  announce( "exiting after not really getting anywhere" );
  exit(EXIT_FAILURE);
}
