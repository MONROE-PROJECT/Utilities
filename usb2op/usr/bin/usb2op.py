#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import urllib2
import simplejson as json

operators = { 
  "Telenor": "op0",
  "NetCom": "op1",
  "242 14": "op2",
  "ICE Nordisk Mobiltelefon AS" : "op2",

  "voda IT": "op0",
  "TIM": "op1",
  "I WIND": "op2",
  "WIND WEB": "op2",

  "Orange": "op0",
  "Orange Internet Móvil": "op0",
  "YOIGO": "op1",
  "Movistar": "op1",
  "voda ES": "op2",

  "TelenorS": "op0",
  "Telenor SE": "op0",
  "Weblink": "op0",
  "Telia": "op1",
  "3 SE": "op2",
  "SWE": "op2",

#NITOS testbed / MONROE-FLEX project
  "460 99": "op0",
#COSMOTE
  "C-OTE": "op0",
}

def usb2op(interface, data=None, reverse=False):
    if interface in ["eth0", "wlan0"]:
        return interface
 
    if data is None:
        r = urllib2.urlopen("http://localhost:88/modems").read()
        data = json.loads(r)

    for modem in data:
        if modem.get('ifname') == 'wwan0':
            continue
        if reverse:
            if operators.get(modem.get('ispName','')) == interface:
                return modem.get('ifname','')
        else:
            if modem.get('ifname','') == interface:  
                op = operators.get(modem.get('ispName',''))
                if op is not None:
                    return op
    return None

if __name__=="__main__":
    reverse=False

    if len(sys.argv) < 2:
        sys.exit(1)
    if sys.argv[1]=="-r":
        reverse=True

    interface = sys.argv[-1]
    op = usb2op(interface, reverse=reverse)
    if op is None:
        sys.exit(1)
    print op
