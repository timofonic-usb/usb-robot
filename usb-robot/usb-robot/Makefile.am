## Process this file with automake to produce Makefile.in

bin_PROGRAMS = usb-robot-slave

usb_robot_slave_SOURCES = output.c control.c main.c control.h libusb.h output.h

EXTRA_DIST = bootstrap

bin_SCRIPTS = usb-robot-master

maintainer-clean-local: 
	$(RM) Makefile.in aclocal.m4 config.h.in stamp-h.in configure

distclean-local:
	$(RM) usb-robot-master

