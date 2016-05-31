Packages
=================
Definitions for packages that can be built as .deb.
Packages with the same name as an existing component (e.g. metadata-exporter) are meant to extend the existing package contents and configuration.

Contents
--------

  * leds-apu - a kernel module to control the LED status on an APU v1
  * metadata-exporter - updated packaging information and utilities for kristrev/data-exporter
  * mf910-switcher - a script to mode switch ZTE MF910 modems
  * monroe-experiments - a cron job to setup the MONROE network namespace and continuous experiments
  * munin-plugins-monroe - small scripts to extend munin-c with MONROE sensors
  * network-test - a script to test cloning interfaces into the MONROE network namespace
  * read_rx_tx - a utility to read rx tx values from interfaces
  * sysevent - a script to send node events to the metadata exporter
  * tuptime - uptime and downtime statistics, from rfrail3/tuptime
  * xtables-addons-cgroup - precompiled iptables cgroup plugin


Extending an existing package
-----------
Copy the folder contents ($FOLDERNAME) into the expanded package ($PKGNAME)

```bash
dpkg -x $PKGNAME.deb pkg
dpkg --control $PKGNAME.deb pkg/DEBIAN
cp -a $FOLDERNAME pkg/
```

Working with packages
-----------


### DEBIAN/conffiles
Lists package configuration files that will not be removed by apt-get remove, and instead requires apt-get purge in order to be removed.

### DEBIAN/control
Package information and controls.

https://www.debian.org/doc/debian-policy/ch-controlfields.html

```
Package: example
Version: 1.0.0
Section: devel
Priority: optional
Architecture: amd64
Depends: libc6 (>= 2.14), libjson-c2 (>= 0.10), libmnl0 (>= 1.0.3-4~), dlb (>=0.1.0), jq (>=1.4), curl (>=7.38)
Installed-Size: 73
Maintainer: John Doe <johndoe@examp.le>
Description: Example package
```

### DEBIAN/md5sums
md5sums for packaged files.

#### Generate md5sums
```bash
cd $PKGNAME

find . -type f ! -regex '.*.hg.*' ! -regex '.*?debian-binary.*' ! -regex '.*?DEBIAN.*' -printf '%P ' | xargs md5sum > DEBIAN/md5sums
```

### Building the .deb package
```bash
dpkg-deb -b pkg/ $PKGNAME.deb
```
