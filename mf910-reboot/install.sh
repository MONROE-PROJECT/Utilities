#!/bin/sh

set -e

echo "Installing reboot script."

DOWNLOAD_LOC="" # HTTP ONLY
wget $DOWNLOAD_LOC/mf910-reboot.sh -O /usr/bin/mf910-reboot.sh
chmod +x /usr/bin/mf910-reboot.sh
wget $DOWNLOAD_LOC/mf910-reboot.init -O /etc/init.d/mf910-reboot
chmod +x /etc/init.d/mf910-reboot
mkdir -p /var/log
ln -s /etc/init.d/mf910-reboot /etc/rc5.d/S99mf910-reboot

echo "Starting reboot script."

/etc/rc5.d/S99mf910-reboot start
