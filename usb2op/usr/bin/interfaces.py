#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import urllib2
import simplejson as json
import subprocess
from usb2op import usb2op
import time

def get_interfaces():
    r = urllib2.urlopen("http://localhost:88/modems").read()
    data = json.loads(r)
    month = time.strftime("%Y-%m")

    modems=[]
    for modem in data:
        iccid = modem.get('iccid', modem.get('mac','n/a'))
        op    = usb2op(modem.get('ifname'), data=data)
        if op is not None:
            host  = None
            netns = None
            try:
                host  = int(open("/monroe/usage/%s/%s.total" % (month, iccid),"r").read().strip())
                netns = int(open("/monroe/usage/netns/%s/%s.total" % (month, iccid),"r").read().strip())
            except:
                pass
            try:
                link = subprocess.check_output(['/sbin/ip', 'netns', 'exec', 'monroe', '/sbin/ip', 'link', 'show', op])
                alive = "state UP" in link
                if alive:
                    modems.append({"iccid":iccid,
                                   "opname":op, "host":host, "netns":netns})
            except:
                continue
    r = urllib2.urlopen("http://localhost:88/dlb").read()
    dlb = json.loads(r)
    interfaces = dlb['interfaces'] 
    for f in interfaces:
        name = f['name']
        mac = f.get('mac')
        if mac:
            op    = usb2op(name, data=data)
            if op:
                modems.append({"opname":op, "mac":mac})
    return modems

if __name__=="__main__":
    print json.dumps(get_interfaces(), indent=2, sort_keys=True)
