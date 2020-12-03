# FreeBSD-xorg-drm-hotplug
FreeBSD xorg-server drm hotplug support

The patches add support for drm hopluggable devices on FreeBSD. 

Previously, when you plug/unplug an external device (let's say USB 3.0 or USB-C) with monitors attached to it while
you are running a Xorg session, a restart for the seesion is required in order to detect the new changes. This is 
particularly annoying when you loose power for a second on your external dock and you have to restart your current 
session in order to re-configure the external monitors.

If you want to give them a try, for libudev-devd it is enough to apply
patch-libudev-devd-drm-hotplug.c, but for xorg-server you need to change
its Makefile to enable udev-kms, and apply patch-xorg-server-drm-bsd-platform.c

Changing

UDEV_CONFIGURE_ON=      --disable-config-udev-kms

to

UDEV_CONFIGURE_ON=      --enabled-config-udev-kms

