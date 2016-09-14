#!/bin/sh

set -e

echo "Installing reboot script."

DOWNLOAD_LOC="" # HTTP ONLY
wget $DOWNLOAD_LOC/dnsmasq.patch -O /tmp/dnsmasq.patch
patch /etc/init.d/dnsmasq /tmp/dnsmasq.patch
wget $DOWNLOAD_LOC/mf910-reboot.sh -O /usr/bin/mf910-reboot.sh
chmod +x /usr/bin/mf910-reboot.sh
wget $DOWNLOAD_LOC/mf910-reboot.init -O /etc/init.d/mf910-reboot
chmod +x /etc/init.d/mf910-reboot
mkdir -p /var/log

echo "Enabling and starting reboot script."
update-rc.d mf910-reboot defaults
/etc/init.d/mf910-reboot start

echo "Finished installation"
