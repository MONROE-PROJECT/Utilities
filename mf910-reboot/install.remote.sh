#!/bin/sh

# This version is to be run on the node,
# mf910 has to be set to adb mode, and be the only device.
set -e

echo "Installing reboot script."

DOWNLOAD_LOC="" # HTTP ONLY
wget $DOWNLOAD_LOC/dnsmasq.patch
adb push ./dnsmasq.patch /tmp/dnsmasq.patch
adb shell 'patch -Np0 /etc/init.d/dnsmasq < /tmp/dnsmasq.patch'

wget $DOWNLOAD_LOC/mf910-reboot.sh
adb push ./mf910-reboot.sh /usr/bin/mf910-reboot.sh
adb shell 'chmod +x /usr/bin/mf910-reboot.sh'

wget $DOWNLOAD_LOC/mf910-reboot.init
adb push ./mf910-reboot.init /etc/init.d/mf910-reboot
adb shell 'chmod +x /etc/init.d/mf910-reboot'
adb shell 'mkdir -p /var/log'

echo "Enabling and starting reboot script."
adb shell 'update-rc.d mf910-reboot defaults'
adb shell '/etc/init.d/mf910-reboot start'

echo "Finished installation"
