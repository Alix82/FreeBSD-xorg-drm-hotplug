--- hw/xfree86/os-support/bsd/bsd_VTsw.c.orig	2020-12-02 17:53:57.431749000 +0100
+++ hw/xfree86/os-support/bsd/bsd_VTsw.c	2020-12-02 17:46:22.016499000 +0100
@@ -101,3 +101,190 @@
     }
     return TRUE;
 }
+
+#ifdef XSERVER_PLATFORM_BUS
+
+#include <xf86drm.h>
+#include <fcntl.h>
+#include <unistd.h>
+#include <errno.h>
+#include <string.h>
+
+#include "xf86.h"
+#include "xf86platformBus.h"
+#include "xf86Bus.h"
+
+static Bool
+get_drm_info(struct OdevAttributes *attribs, char *path, int delayed_index)
+{
+    drmVersionPtr v;
+    int fd;
+    int err = 0;
+
+    LogMessage(X_INFO, "Platform probe for %s\n", attribs->syspath);
+ 
+    fd = open(path, O_RDWR | O_CLOEXEC, 0);
+
+    if (fd == -1)
+        return FALSE;
+
+    /* for a delayed probe we've already added the device */
+    if (delayed_index == -1) {
+            xf86_add_platform_device(attribs, FALSE);
+            delayed_index = xf86_num_platform_devices - 1;
+    }
+
+    v = drmGetVersion(fd);
+    if (!v) {
+        xf86Msg(X_ERROR, "%s: failed to query DRM version\n", path);
+        goto out;
+    }
+
+    xf86_platform_odev_attributes(delayed_index)->driver = XNFstrdup(v->name);
+    drmFreeVersion(v);
+
+out:
+    close(fd);
+    return (err == 0);
+}
+
+void
+xf86PlatformReprobeDevice(int index, struct OdevAttributes *attribs)
+{
+    Bool ret;
+    char *dpath = attribs->path;
+
+    ret = get_drm_info(attribs, dpath, index);
+    if (ret == FALSE) {
+        xf86_remove_platform_device(index);
+        return;
+    }
+    ret = xf86platformAddDevice(index);
+    if (ret == -1)
+        xf86_remove_platform_device(index);
+}
+
+void
+xf86PlatformDeviceProbe(struct OdevAttributes *attribs)
+{
+    int i;
+    char *path = attribs->path;
+    Bool ret;
+
+    if (!path)
+        goto out_free;
+
+    for (i = 0; i < xf86_num_platform_devices; i++) {
+        char *dpath = xf86_platform_odev_attributes(i)->path;
+
+        if (dpath && !strcmp(path, dpath))
+            break;
+    }
+
+    if (i != xf86_num_platform_devices)
+        goto out_free;
+
+    LogMessage(X_INFO, "xfree86: Adding drm device (%s)\n", path);
+
+    if (!xf86VTOwner()) {
+            /* if we don't currently own the VT then don't probe the device,
+               just mark it as unowned for later use */
+            xf86_add_platform_device(attribs, TRUE);
+            return;
+    }
+
+    ret = get_drm_info(attribs, path, -1);
+    if (ret == FALSE)
+        goto out_free;
+
+    return;
+
+out_free:
+    config_odev_free_attributes(attribs);
+}
+
+
+void NewGPUDeviceRequest(struct OdevAttributes *attribs)
+{
+    int old_num = xf86_num_platform_devices;
+    int ret;
+
+    xf86PlatformDeviceProbe(attribs);
+
+    if (old_num == xf86_num_platform_devices)
+        return;
+
+    if (xf86_get_platform_device_unowned(xf86_num_platform_devices - 1) == TRUE)
+        return;
+
+    ret = xf86platformAddDevice(xf86_num_platform_devices-1);
+    if (ret == -1)
+        xf86_remove_platform_device(xf86_num_platform_devices-1);
+
+    ErrorF("xf86: found device %d\n", xf86_num_platform_devices);
+}
+
+
+void DeleteGPUDeviceRequest(struct OdevAttributes *attribs)
+{
+    int index;
+    char *syspath = attribs->syspath;
+
+    if (!syspath)
+        goto out;
+
+    for (index = 0; index < xf86_num_platform_devices; index++) {
+        char *dspath = xf86_platform_odev_attributes(index)->syspath;
+        if (dspath && !strcmp(syspath, dspath))
+            break;
+    }
+
+    if (index == xf86_num_platform_devices)
+        goto out;
+
+    ErrorF("xf86: remove device %d %s\n", index, syspath);
+
+    if (xf86_get_platform_device_unowned(index) == TRUE)
+            xf86_remove_platform_device(index);
+    else
+            xf86platformRemoveDevice(index);
+out:
+    config_odev_free_attributes(attribs);
+}
+
+Bool
+xf86PlatformDeviceCheckBusID(struct xf86_platform_device *device, const char *busid)
+{
+    const char *syspath = device->attribs->syspath;
+    BusType bustype;
+    const char *id;
+
+    if (!syspath)
+        return FALSE;
+
+    bustype = StringToBusType(busid, &id);
+    if (bustype == BUS_PCI) {
+        struct pci_device *pPci = device->pdev;
+        if (xf86ComparePciBusString(busid,
+                                    ((pPci->domain << 8)
+                                     | pPci->bus),
+                                    pPci->dev, pPci->func)) {
+            return TRUE;
+        }
+    }
+    else if (bustype == BUS_PLATFORM) {
+        /* match on the minimum string */
+        int len = strlen(id);
+
+        if (strlen(syspath) < strlen(id))
+            len = strlen(syspath);
+
+        if (strncmp(id, syspath, len))
+            return FALSE;
+        return TRUE;
+    }
+    return FALSE;
+}
+
+#endif
+
--- config/udev.c.orig	2020-12-02 17:50:58.019695000 +0100
+++ config/udev.c	2020-12-02 17:49:02.355349000 +0100
@@ -491,6 +491,7 @@
 
 #ifdef CONFIG_UDEV_KMS
 
+#ifndef __FreeBSD__
 /* Find the last occurrence of the needle in haystack */
 static char *strrstr(const char *haystack, const char *needle)
 {
@@ -515,6 +516,7 @@
 
     return last;
 }
+#endif
 
 static void
 config_udev_odev_setup_attribs(struct udev_device *udev_device, const char *path, const char *syspath,
@@ -530,10 +532,16 @@
     attribs->minor = minor;
 
     value = udev_device_get_property_value(udev_device, "ID_PATH");
+#ifndef __FreeBSD__
     if (value && (str = strrstr(value, "pci-"))) {
         attribs->busid = XNFstrdup(str);
         attribs->busid[3] = ':';
     }
+#else
+    str = value;
+    attribs->busid = XNFstrdup(str);
+    attribs->busid[3] = ':';
+#endif
 
     /* ownership of attribs is passed to probe layer */
     probe_callback(attribs);
