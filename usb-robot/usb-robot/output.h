#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdlib.h>
#include <stdio.h>

extern char const* program_name;

extern void
announce(const char *format,...)
     __attribute__((format (printf, 1, 2)))
     ;

extern     
void
message( const char *format, ...)
     __attribute__((format (printf, 1, 2)))
     ;

extern     
void
error( const char *format, ...)
     __attribute__((format (printf, 1, 2)))
     ;

extern     
void
usb_error( const char *format, ...)
     __attribute__((format (printf, 1, 2)))
     ;
     
extern void
internal_error_func(const char* file, int line, const char *format,
	 ...)
     __attribute__((format (printf, 3, 4)))
     ;
extern void
panic_func( const char* file, int line, const char *format,
	 ...)
     __attribute__((format (printf, 3, 4)))
     __attribute__((noreturn))
     ;

     // GNU C extension ...
#define internal_error(x,args...) internal_error_func( __FILE__, __LINE__, x,##args )
     
#define panic(x,args...) panic_func( __FILE__, __LINE__, x, ##args )
     
#define cant_get_here() panic("the impossible has happened!")

#endif
