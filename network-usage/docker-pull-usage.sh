#!/bin/bash

iptables -I OUTPUT 1 -p tcp --destination-port 443 -m owner --gid-owner 0 -j ACCEPT
iptables -Z OUTPUT 1
iptables -I INPUT 1 -p tcp --source-port 443 -j ACCEPT
iptables -Z INPUT 1

docker pull $1 &>/dev/null

SENT=$(iptables -vxL OUTPUT | grep "https" | awk '{print $2}')
RECEIVED=$(iptables -vxL INPUT | grep "https" | awk '{print $2}')
SUM=$(($SENT + $RECEIVED))

echo "$SUM bytes."
echo "$SENT sent, $RECEIVED received."

iptables -D OUTPUT -p tcp --destination-port 443 -m owner --gid-owner 0 -j ACCEPT
iptables -D INPUT  -p tcp --source-port 443 -j ACCEPT

