#!/bin/bash 

## parameters: $1 the interface to test 

set -e
MONROEIP=http://128.39.37.140
MONROEURL=http://www.monroe-project.eu

for IF in $1; do
  echo "## MAIN INTERFACE ##"
  ADDR=$(ip -4 addr show $IF | grep -oP "(?<=inet ).*(?=/)")
  echo "present DHCP address: $ADDR"

  echo "== PING TEST =="
  ping -c 3 -I$IF 8.8.8.8
  echo "== CURL TEST =="
  curl -o network-test.html -m 30 -L --interface $IF $MONROEIP || echo "Failed." || true
  ip link show $IF

  echo "## MACVLAN INTERFACE ##"
  ip link add link $IF tmp type macvlan
  trap "ip link del tmp" EXIT
  ifconfig tmp up
  #todo: wait for dhcp
  sleep 5 
  NADDR=$(ip -4 addr show tmp | grep -oP "(?<=inet ).*(?=/)")
  echo "acquired DHCP address: $NADDR"

  echo "== PING TEST =="
  ping -c 3 -Itmp 8.8.8.8
  echo "== CURL TEST =="
  curl -o network-test.html -m 30 -L --interface $IF $MONROEIP || echo "Failed." || true
  ip link show tmp

  MNS="ip netns exec monroe"

  ip link set tmp netns monroe
  trap "$MNS ip link del tmp" EXIT

  $MNS ifconfig tmp up
  sleep 5
  NSADDR=$($MNS ip -4 addr show tmp | grep -oP "(?<=inet ).*(?=/)")
  echo "acquired DHCP address: $NSADDR"

  echo "## MACVLAN INTERFACE IN MONROE NETNS WITH DHCP ACQUIRED IP ##"
  #$MNS ifconfig tmp $NSADDR
  echo "== PING TEST =="
  $MNS ping -c 3 -Itmp 8.8.8.8
  echo "== CURL TEST =="
  $MNS curl -o network-test.html -m 30 -L --interface tmp $MONROEIP || echo "Failed." || true
  $MNS ip link show tmp

  echo "## MACVLAN INTERFACE IN MONROE NETNS WITH IDENTICAL IP ##"
  $MNS ifconfig tmp $NADDR
  echo "== PING TEST =="
  $MNS ping -c 3 -Itmp 8.8.8.8
  echo "== CURL TEST =="
  $MNS curl -o network-test.html -m 30 -L --interface tmp $MONROEIP || echo "Failed." || true
  $MNS ip link show tmp

  rm network-test.html
done
