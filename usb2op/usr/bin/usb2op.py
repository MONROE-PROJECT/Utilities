#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import urllib2
import simplejson as json

def usb2op(interface, data=None, reverse=False):
    if interface in ["eth0", "wlan0", "wlan1"]:
        return interface
 
    if data is None:
        r = urllib2.urlopen("http://localhost:88/modems").read()
        data = json.loads(r)
        
    config=[]
    match=None
    
    try:
        config=open("/tmp/interfaces","r").read().splitlines()
    except:
    	pass

    if reverse:
    	index=int(interface[2:])
        if len(config)<=index:
        	return None
        rimei=config[index]
    
    for modem in data:
        imei = modem.get('imei')
        if imei is None:
        	continue
        
        if reverse:
        	if imei == rimei:
        	    return modem.get('ifname')
        else:
            if modem.get('ifname','') == interface:  
                if imei not in config:
                	config.append(imei)
                opnr = config.index(imei)
                match = "op%i" % opnr
                break
                
    fd=open("/tmp/interfaces", "w") 
    for line in config:
    	fd.write("%s\n" % line) 
    fd.close()
    return match

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
