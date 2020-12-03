diff -Naur udev-monitor.c.orig udev-monitor.c
--- udev-monitor.c.orig	2020-12-02 17:27:51.984946000 +0100
+++ udev-monitor.c	2020-12-02 17:28:01.950674000 +0100
@@ -279,7 +279,7 @@
 	if (!um)
 		return (NULL);
 
-	if (pipe2(um->fds, O_CLOEXEC) == -1) {
+	if (pipe2(um->fds, O_CLOEXEC | O_NONBLOCK) == -1) {
 		ERR("pipe2 failed");
 		free(um);
 		return (NULL);
diff -Naur udev-utils.c.orig udev-utils.c
--- udev-utils.c.orig	2020-12-02 17:27:51.985347000 +0100
+++ udev-utils.c	2020-12-02 17:28:01.950965000 +0100
@@ -640,6 +640,14 @@
 void
 create_drm_handler(struct udev_device *ud)
 {
+	char buf[32], busid[32];
+	size_t busid_len;
+
 	udev_list_insert(udev_device_get_properties_list(ud), "HOTPLUG", "1");
 	set_parent(ud);
+
+	snprintf(buf, 32, "hw.dri.%s.busid", udev_device_get_sysnum(ud));
+
+	sysctlbyname(buf, busid, &busid_len, NULL, 0);
+	udev_list_insert(udev_device_get_properties_list(ud), "ID_PATH", busid);
 }
