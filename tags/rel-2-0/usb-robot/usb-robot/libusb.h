
#ifndef LIBUSB_H
#define LIBUSB_H

/* Most of the libusb-0.1.0 functions are not exported for some reason */

#include <usb.h>

extern int usb_set_configuration(struct usb_dev_handle *dev, int configuration);
extern int usb_claim_interface(struct usb_dev_handle *dev, int interface);
/*extern char* usb_error_str;*/

#ifndef USB_OK
#define USB_OK 0
#endif

#ifndef USB_DIR_IN
#define USB_DIR_IN 0x80
#endif

#ifndef USB_DIR_OUT
#define USB_DIR_OUT 0x00
#endif

#endif

