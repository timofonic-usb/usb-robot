#include <stdarg.h>
#include <stdio.h>

#include "output.h"

#include "libusb.h"

void
internal_error_func (const char* file, int line, const char *format,
	 ...)
{
  va_list args;

  fprintf( stderr, "%s: internal error: ", program_name );

  va_start (args,format);
  vfprintf( stderr, format, args);
  va_end (args);

  fprintf( stderr,"(%s,%d)\n",file,line );
}

void
panic_func( const char* file, int line, const char *format,
	 ...)
{
  va_list args;

  fprintf( stderr, "%s: panic: ", program_name );

  va_start (args,format);
  vfprintf( stderr, format, args);
  va_end (args);

  fprintf( stderr," in %s line %d\n",file,line );
  
  abort();
}


void
announce(const char *format,...)
{
  va_list args;

  fprintf( stderr, "%s: ", program_name );

  va_start (args,format);
  vfprintf( stderr, format, args);
  va_end (args);

  fprintf( stderr,"\n" );
}

void
message( const char *format, ...)
{     
  va_list args;

  va_start (args,format);
  vfprintf( stderr, format, args);
  va_end (args);

  fprintf( stderr,"\n" );
}


void
error( const char *format, ...)
{     
  va_list args;

  fputs( "error: ", stderr );

  va_start (args,format);
  vfprintf( stderr, format, args);
  va_end (args);

  fputs( "\n",stderr );
}

void
usb_error( const char *format, ...)
{     
  va_list args;

  fputs( "status: ", stderr );

  va_start (args,format);
  vfprintf( stderr, format, args);
  va_end (args);

  fprintf( stderr,"\nusb error: %s\n", usb_strerror());
}
