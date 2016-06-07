#!/bin/bash

if [ ! $(getent group pnu) ]; then 
  groupadd pnu
fi

iptables -I OUTPUT 1 -m owner --gid-owner pnu -j ACCEPT
iptables -Z OUTPUT 1
# NOTE: Reply packets are not accounted for

sg pnu "$1"

iptables -vxL OUTPUT | grep "owner GID match pnu" | awk '{print $2}'
iptables -D OUTPUT -m owner --gid-owner pnu -j ACCEPT
