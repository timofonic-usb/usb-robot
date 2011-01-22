#ifndef CONTROL_H
#define CONTROL_H

struct usb_dev_handle;

// returns 0 on success
extern int control_device(struct usb_dev_handle *handle);

#endif
