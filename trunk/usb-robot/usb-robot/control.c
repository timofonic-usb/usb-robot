#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "config.h"

#if !defined(HAVE_READLINE) && defined(HAVE_LIBREADLINE) && defined(HAVE_READLINE_READLINE_H)
#define HAVE_READLINE 1
#else
#define HAVE_READLINE 0
#endif

#if HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "output.h"
#include "libusb.h"


typedef enum
{
  transfer_bulk,
  transfer_ctrl
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


struct command_context_struct;

typedef char* (*data_reader)(struct command_context_struct* context,size_t size);
typedef void (*data_writer)(struct command_context_struct* context,size_t size,char* buffer);

typedef struct
{
  char* name;
  data_reader proc;
}
reader_wrapper;

typedef struct
{
  char* name;
  data_writer proc;
}
writer_wrapper;


typedef struct command_context_struct
{
  struct usb_dev_handle* device;
  FILE* in;
  FILE* out;
  data_reader read;
  data_writer write;
  long id;
} command_context
;
typedef int (*command_action)( command_context* context, char*buffer );


typedef struct 
{
  char* name;
  char* help;
  command_action action;
}
command;

static
char*
data_read_binary(command_context* context, size_t size )
{
  char* buffer = malloc( size );

  assert( buffer );

  if ( fread (buffer, 1, size, context->in ) != size )
    panic( "error reading input from UNIX pipe/stream" );

  return buffer;
}

static
char*
data_read_hex(command_context* context, size_t size )
{
  int c,i=0;
  char* buffer = malloc( size );

  /* FIXME: there is no way to cancel the transfer, so user has fill
     up the, say, 100000000 byte buffer somehow ;-) */
  
  assert( buffer );

  fprintf( context->out,"Enter data in format FF FB 00 FC\n");
  
  fflush(0);
 read_data:
  for(i=0;i<size;i++)
    {
      if(fscanf(context->in,"%2x", &c)!=1){
	fprintf( context->out, "USERERROR: bad input, re-enter all data\n");
	goto read_data;
      }
      
      buffer[i] = c;
    }
  return buffer;
}

static const reader_wrapper readers[] = 
{
  {"hex",data_read_hex
  },
  {
    "binary",data_read_binary
  },
  {
    0,0
  }
}
;

  

static
void
data_write_binary(command_context* context, size_t size, char* buffer )
{
  if ( fwrite (buffer, 1, size, context->out ) != size )
    panic( "error writing output to UNIX pipe/stream" );

  fflush(0);
}

static
void
data_write_hex( command_context* context, size_t size, char* buffer )
{
  int i;
  const int line_size=16;
  int line_end;
  
  for (i=0; i<size; )
    {
      line_end = i+line_size;
      if(line_end>=size)
	line_end=size;

      fprintf( context->out, "%04x: ", i);
      for(;i<line_end;i++)
	fprintf( context->out, "%02x ", (int)(unsigned char)buffer[i]);
      fputc('\n',context->out);
    }
  
  fflush(0);
}

static const writer_wrapper writers[] = 
{
  {"hex",data_write_hex
  },
  {
    "binary",data_write_binary
  },
  {
    0,0
  }
}
;


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
command_change_output( command_context* context, char*buffer )
{
  writer_wrapper const* rw;
  
  for(rw=writers;rw->name;rw++)
    {
      if(*buffer == *rw->name)
	{
	  context->write = rw->proc;
	  fprintf( context->out, "Output format changed to %s\n",rw->name);
	  return 0;
	}
    }
  fprintf( context->out, "USERERROR: bad value for output encoder: %s\n",
	   buffer);
  
  return -2;
}

static
int
command_change_input( command_context* context, char*buffer )
{
  reader_wrapper const* rw;
  
  for(rw=readers;rw->name;rw++)
    {
      if(*buffer == *rw->name)
	{
	  context->read = rw->proc;
	  fprintf( context->out, "Input format changed to %s\n",rw->name);
	  return 0;
	}
    }
  fprintf( context->out, "USERERROR: bad value for input decoder %s\n",
	   buffer);
  
  return -2;
}


static
int
command_set_config( command_context* context, char*buffer )
{
  int confignumber;

  confignumber = read_integer( buffer );
  if ( (usb_set_configuration( context->device, confignumber)) != USB_OK )
    {
      usb_error( "problem setting config %d", confignumber );
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
      usb_error( "problem setting claiming interface %d", interface );
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
    case 'c': *ttype = transfer_ctrl;
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
  
  long size = 0;
  long read;
  long ep = -1;
  direction dir = dir_out;
  char* data;
  int return_value = 0;
  transfer_type ttype = transfer_bulk;
  int requesttype = ((dir == dir_in) ? USB_DIR_IN : USB_DIR_OUT)
    | USB_TYPE_VENDOR | USB_RECIP_INTERFACE;
  int request = 0, value = 0, index = 0;
  
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
      "requesttype", param_int, &requesttype 
    },
    {
      "request", param_int, &request 
    },
    {
      "value", param_int, &value 
    },
    {
      "index", param_int, &index 
    },
    {
      0,0,0
    }
  }
  ;
  parameter const* param;
  char const* val;
  
  for( param = parameters; param->name; param++ )
    {
      val = read_param(param->name, buffer);
      if ( val )
	{
	  if( param->action( val, param->value ) )
	    {
	      fprintf( context->out, "USERERROR: bad value for parameter %s\n",
		       param->name );
	      return -2;
	    }
	}
    }
  
  switch(dir)
    {
    case dir_in: assert( data = malloc( size ) );break;
    case dir_out: data = context->read( context, size );break;
    default: cant_get_here();
    }

  switch(ttype)
    {
    case transfer_ctrl:
      message( "doing control message id %d %s device, size %d, timeout %d frames, %x:%x:%x:%x",
	       (int)context->id,
	       direction_to_string[dir],
	       (int)size,
	       (int)timeout, requesttype, request, value, index );

      if (usb_control_msg(context->device, requesttype, request, value, index, data, size, timeout)) {
	usb_error( "problem doing control msg");
	return_value = -1;
	break;
      }
      if (dir == dir_in) {
	fprintf( context->out, "DATA: id=%d length=%d\n", (int)context->id,
		 (int)size );
	
	context->write( context,size,data );
      }
      return_value = 0;
      break;
 
    case transfer_bulk:
      message( "doing bulk transfer id %d %s ep 0x%x, size %d, timeout %d frames",
	       (int)context->id,
	       direction_to_string[dir],
	       (int)ep,
	       (int)size,
	       (int)timeout );

      if ( ep == -1 )
	{
	  fprintf( context->out, "USERERROR: no value for ep given\n" );
	  return -2;
	}
      switch(dir)
	{
	case dir_in:
	  if ( (read = usb_bulk_read(context->device, ep, data, size, timeout)) != size )
	    {
	      if ( read > 0 )
		usb_error( "problem doing read; only %d read from %d", (int)read, (int)size );
	      else
		usb_error( "problem doing read" );
	      
	      return_value = -1;
	      break;
	    }
	  else
	    {
	      message( "read id %d successful", (int)context->id );
	      fprintf( context->out, "DATA: id=%d length=%d\n", (int)context->id,
		       (int)size );
	      context->write( context, size, data );
	    }
	  break;
	case dir_out:
	  if ( usb_bulk_write(context->device, ep, data, size, timeout) != USB_OK )
	    {
	      usb_error( "problem doing write" );
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
command_help( command_context* context, char*buffer );

static const command commands[] = 
{ {
  "transfer",
  "make a USB transfer. Parameters:\n"
  "\ttype=[\"bulk\",\"control\"]\n"
  "\tep=[endpoint number]\n"
  "\tdir=[\"in\",\"out\"]\n"
  "\ttimeout=[frames]\n"
  "\trequesttype=[int] -- only meaningful for control transfers\n"
  "\trequest=[int] -- only meaningful for control transfers\n"
  "\tvalue=[int] -- only meaningful for control transfers\n"
  "\tindex=[int] -- only meaningful for control transfers\n"
  "\tExample: transfer type=bulk size=6 ep=0x01 dir=OUT\n"
  ,
  command_transfer,
},
  {
    "config", "select a configuration, parameter [config number]", command_set_config
  },
  {
    "interface", "claim an interface, parameter [interface number]",command_claim_interface
  },
  {
    "decoding","change decoding of data input from console\n"
    "\tPossible parameters: binary,hex",
    command_change_input
  },
  {
    "encoding","change encoding of data output to console\n"
    "\tPossible parameters: binary,hex",
    command_change_output
  },
  {
    "abort", "try for another device with matching ids", command_abort
  },
  {
    "help", "print command help", command_help
  },
  {
    "quit", "leave the program", command_quit
  },
  {
    0,0,0
  }
}
;


static
int
command_help( command_context* context, char*buffer ){
  command const* c;

  fprintf(context->out,
	  "The purpose of this program is to let you communicate\n"
	  "directly with USB devices. The syntax is a command name\n"
	  "possibly followed by parameters on the same line. The\n"
	  "following commands are recognised:\n"
	  );
  
  for(c=commands;c->help;c++)
    fprintf(context->out,"%s -- %s\n",c->name,c->help);
      
  return 0;
}

static
void
ub_getline(char**buffer,size_t*buffer_length, command_context* context)
{
  ssize_t length = 0;

  while(length<=0){
    if(HAVE_READLINE && isatty(fileno(context->in))) {
      rl_instream = context->in;
      rl_outstream = context->out;
      
      if(*buffer)free(*buffer);
      *buffer = readline("usb-robot> ");
      if(!*buffer)
	panic("error reading next command from readline");
      *buffer_length = length = strlen(*buffer);
      if (**buffer)
	add_history(*buffer);
    } else{
      /* getline is a GNU extension */
      length = getline(buffer, buffer_length, context->in);
      
      if ( length == -1 )
	panic("error reading next command from UNIX pipe/stream");
    }
  }
}


static
int
do_command( command_context* context )
{
  char* buffer = 0;
  size_t buffer_length = 0;
  int return_value = 0;
  
  command const* i;
  
  ub_getline(&buffer, &buffer_length, context);

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
      handle, stdin, stdout, data_read_binary, data_write_binary, 1
    }
  ;

  fprintf( context.out, "OK: id=0\n" );
  fflush( context.out );

  if(isatty(fileno(context.in)))
    message("Type help and press return for a list of commands");
  
  while( !(status = do_command( &context ) ) )
    context.id++;

  if ( status < 0 ) // error
    panic("unhandled error dealing with user request");
  
  if ( status == 2 ) // defer
    return 0;
  
  return 1;
}
