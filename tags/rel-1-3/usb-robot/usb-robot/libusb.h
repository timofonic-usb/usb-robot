
#ifndef LIBUSB_H
#define LIBUSB_H

/* Most of the libusb-0.1.0 functions are not exported for some reason */

#include <usb.h>

extern void usb_find_busses();
extern void usb_find_devices();
extern int usb_set_configuration(struct usb_dev_handle *dev, int configuration);
extern int usb_claim_interface(struct usb_dev_handle *dev, int interface);


#endif

