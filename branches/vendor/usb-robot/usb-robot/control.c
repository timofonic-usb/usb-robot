#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "output.h"
#include "libusb.h"
#include "config.h"

typedef enum
{
  transfer_bulk
}
transfer_type;

typedef enum 
{
  dir_undefined=0,
  dir_in,
  dir_out
}
direction;

static const char* direction_to_string
[] = 
  {
    0, // shouldn't happen
    "from",
    "to"
  }
;

typedef struct
{
  struct usb_dev_handle* device;
  FILE* in;
  FILE* out;
  long id;
} command_context
;
typedef int (*command_action)( command_context* context, char*buffer );


typedef struct 
{
  char* name;
  command_action action;
}
command;

static
char*
read_binary( FILE* stream, size_t size )
{
  char* buffer = malloc( size );

  assert( buffer );

  if ( fread (buffer, 1, size, stream ) != size )
    panic( "error reading input from UNIX pipe/stream" );

  return buffer;
}

static
void
write_binary( FILE* stream, size_t size, char* buffer )
{
  if ( fwrite (buffer, 1, size, stream ) != size )
    panic( "error writing output to UNIX pipe/stream" );

  fflush(0);
}


static
char*
next_word_start( char* buffer )
{
  char*pointer=buffer;
  
  while(!isspace(*pointer))
    pointer++;
	
  while (isspace(*pointer))
    pointer++;
  
  return pointer;
}


static
long
read_integer( char const* buffer )
{
  return strtol(buffer,0,0);
}

static
char const*
read_param( char const* name, char const* buffer )
{
  char const* pointer=buffer;

  do 
    {
      pointer = strstr (pointer,name);
      if ( !pointer )
	return 0;

      pointer += strlen(name);
    }
  while( *pointer != '=' );
  
  return pointer+1;
}



static
int
command_set_config( command_context* context, char*buffer )
{
  int confignumber;
  confignumber = read_integer( buffer );
  if ( usb_set_configuration( context->device, confignumber) != USB_OK )
    {
      error( "problem setting config %d", confignumber );
      return -1;
    }
  return 0;
}

static
int
command_claim_interface(command_context* context, char*buffer )
{
  int interface;
  interface = read_integer( buffer );
  if ( usb_claim_interface( context->device, interface ) != USB_OK )
    {
      error( "problem setting claiming interface %d", interface );
      return -1;
    }
  return 0;
}

static
int
command_abort( command_context* context, char*buffer )
{
  message( "aborting on request and looking for another device" );
  return 2;
}

static
int
command_quit( command_context* context, char*buffer )
{
  message( "quiting on request" );
  return 1;
}

typedef int (*param_action) ( char const* buffer, void*value );

typedef struct
{
  char* name;
  param_action action;
  void* value;
} parameter;


static
int
param_int( char const* buffer, void*value )
{
  long* integer = (long*)value;
  *integer = read_integer(buffer);
  return 0;
}

static
int
param_char( char const* buffer, void*value )
{
  char* character = (char*)value;
  *character = *buffer;
  return 0;
}

static
int
param_direction( char const* buffer, void*value )
{
  direction* dir = (direction*)value;
  switch(tolower(*buffer))
    {
    case 'o': *dir = dir_out;
      break;
    case 'i': *dir = dir_in;
      break;
    default: error( "bad initial direction letter, %c", *buffer );
      return -1;
    }
  
  return 0;
}
static
int
param_transfer_type( char const* buffer, void*value )
{
  transfer_type* ttype = (transfer_type*)value;
  switch(tolower(*buffer))
    {
    case 'b': *ttype = transfer_bulk;
      break;
    default:
      return -1;
    }
  
  return 0;
}


static
int
command_transfer( command_context* context, char*buffer )
{
  long timeout = 10000;
  // if it hasn't happened after 10 seconds it probably won't ever
  
  long size = -1;
  long read;
  long ep = -1;
  direction dir = dir_out;
  char* data;
  int return_value = 0;
  transfer_type ttype = transfer_bulk;
  
  const parameter parameters[] = 
  {
    {
      "timeout",param_int,&timeout
    },
    {
      "size",param_int,&size
    },
    {
      "ep",param_int,&ep
    },
    {
      "dir",param_direction,&dir
    },
    {
      "type", param_transfer_type, &ttype 
    },
    {
      0,0,0
    }
  }
  ;
  parameter const* param;
  char const* value;
  
  for( param = parameters; param->name; param++ )
    {
      value = read_param(param->name, buffer);
      if ( value )
	{
	  if( param->action( value, param->value ) )
	    {
	      fprintf( context->out, "USERERROR: bad value for parameter %s\n",
		       param->name );
	      return -2;
	    }
	}
    }
  
  if ( ep == -1 )
    {
      fprintf( context->out, "USERERROR: no value for ep given\n" );
      return -2;
    }
  if ( size == -1 )
    {
      fprintf( context->out, "USERERROR: no value for size given\n" );
      return -2;
    }
  
  
  switch(dir)
    {
    case dir_in: assert( data = malloc( size ) );break;
      
    case dir_out: data = read_binary( context->in, size );break;
      
    default: cant_get_here();
    }

  message( "doing bulk transfer id %d %s ep 0x%x, size %d, timeout %d frames",
	   (int)context->id,
	   direction_to_string[dir],
	   (int)ep,
	   (int)size,
	   (int)timeout );
      
  switch(ttype)
    {
    case transfer_bulk:
      switch(dir)
	{
	case dir_in:
	  if ( (read = usb_bulk_read(context->device, ep, data, size, timeout)) != size )
	    {
	      if ( read > 0 )
		error( "problem doing read; only %d read from %d", (int)read, (int)size );
	      else
		error( "problem doing read" );
	      
	      return_value = -1;
	      break;
	    }
	  else
	    {
	      message( "read id %d successful", (int)context->id );
	      fprintf( context->out, "DATA: id=%d length=%d\n", (int)context->id,
		       (int)size );
	      write_binary( context->out, size, data );
	    }
	  break;
	case dir_out:
	  if ( usb_bulk_write(context->device, ep, data, size, timeout) != USB_OK )
	    {
	      error( "problem doing write" );
	      return_value = -1;
	    }
	  else
	    message( "write id %d successful", (int)context->id );
	  break;
	default:cant_get_here();
	  
	}
      break;
    default: cant_get_here();
    }

 cleanup:
  free(data);
  return return_value;
}


static
int
do_command( command_context* context )
{
  ssize_t length = 0;
  char* buffer = 0;
  size_t buffer_length = 0;
  int return_value = 0;
  
  const command commands[] = 
  {
    {
      "transfer", command_transfer 
    },
    {
      "config", command_set_config
    },
    {
      "interface", command_claim_interface
    },
    {
      "abort", command_abort
    },
    {
      "quit", command_quit
    },
    {
      0,0
    }
  }
  ;
  command const* i;
  
  while(length <= 1)
    {
      /* GNU extension */
      length = getline(&buffer, &buffer_length, context->in);

      if ( length == -1 )
	{
	  panic("error reading next command from UNIX pipe/stream");
	}
    }

  for( i = commands; i->name; i++ )
    {
      if ( strncasecmp( i->name, buffer, 1 ) ) /*strlen( i->name ) ) )*/
	   continue;

      switch( i->action( context, next_word_start(buffer ) ) )
	{
	case 0: fprintf( context->out, "OK: id=%d\n", (int)context->id );
	  goto cleanup;
	case -2: // syntax error in command
	case -1: fprintf( context->out, "ERROR: id=%d\n", (int)context->id );
	  goto cleanup;
	case 2: // abort
	  return_value = 2;
	  goto cleanup;
	case 1: // quit
	  return_value = 1;
	  goto cleanup;
	default: cant_get_here();
	}
    }
	  

  fprintf( context->out, "USERERROR: unrecognised command \"%s\"\n", buffer );
  return_value = 0;
  
 cleanup:
  fflush(0);

  if ( buffer_length )
    free( buffer );
  return return_value;
}

  

int
control_device( struct usb_dev_handle* handle )
{
  int status;
  command_context context = 
  {
    handle, stdin, stdout, 1
  }
  ;
  fprintf( context.out, "OK: id=0\n" );
  fflush( context.out );
  
  while( !(status = do_command( &context ) ) )
    context.id++;

  if ( status < 0 ) // error
    panic("unhandled error dealing with user request");
  
  if ( status == 2 ) // defer
    return 0;
  
  return 1;
}
