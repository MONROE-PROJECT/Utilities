#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import urllib2
import simplejson as json
import subprocess
from usb2op import usb2op

def get_interfaces():
    r = urllib2.urlopen("http://localhost:88/modems").read()
    data = json.loads(r)

    modems=[]
    for modem in data:
        iccid = modem.get('iccid')
        op    = usb2op(modem.get('ifname'), data=data)
        if op is not None:
            try:
                link = subprocess.check_output(['/sbin/ip', 'netns', 'exec', 'monroe', '/sbin/ip', 'link', 'show', op])
                alive = "state UP" in link
                if alive:
                    modems.append({"iccid":iccid,
                                   "opname":op})
            except:
                continue
    return modems

if __name__=="__main__":
    print json.dumps(get_interfaces())
