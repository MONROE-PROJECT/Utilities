MBB ping
=====================

MBB ping supports a subset of the functionality of the normal ping-utilities and
were written as an exercise for me to get familiar with raw sockets and
BPF-filters, as well as supporting some uncommon features that make measuring
MBB networks easier. One example is that the tool can be configured to use
different packet sizes for the first and the following requests. This can be
used to trigger state changes in 3G networks. In order to see all supported
options, use the -h option.

The code itself is quite straight forward. First, we create a raw socket which
is bound to an interface and optionally a local IP. We add a BPF filter to the
socket to avoid receiving other ICMP packets that are received on the machine,
and connect the socket to the destination address.

After that, the event loop is started and we send ICMP ECHOs at a given interval
(default is to wait five seconds after first message, then one second). When
a packet is received and checksum is correct, we compute the rtt and output the
information. Default behavior is that we send an infinite number of ECHO
messages.

In order for this tool to be used as a full MONROE example, we should use an
event loop library like libuv and implement proper callbacks, etc. However, this
will add a lot of skeleton code and might make the code more difficult to
understand. Instead, we can do the following since this just be a quite straight
forward example:

* Create a Netlink socket that will monitor the kernel network events. This
  can be used to monitor when interfaces become available, get internal IP, etc.
* Create a ZMQ socket. This socket will be used to receive the data exported by
  the metadata exporter.
* Add these sockets to the event loop and store the sockets in the
  epoll\_event.data.
* When we receive data, check if data origins from Netlink or ZMQ socket and
  call functions for handling this data. If socket with event is neither, then
  it is one of the sockets used for ICMP.
* ICMP-sockets are created dynamically as interfaces come up and down. We need
  to have a data structure per interface because of the metadata, so store fd in
  this structure. Do looks when we receive data to get interface information.
