#ifndef CONTROL_H
#define CONTROL_H

struct usb_dev_handle;

extern
int // return 0 on success
control_device( struct usb_dev_handle* handle )
     ;

#endif
