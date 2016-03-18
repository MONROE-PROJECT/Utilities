#!/bin/bash

if [ -x /usr/bin/sysevent ]; then
    # send event on logout
    trap '/usr/bin/sysevent -t "Maintenance.Stop" -k "user" -v "$USER"; exit' 0;
    # and now on login
    /usr/bin/sysevent -t "Maintenance.Start" -k "user" -v "$USER" 2>/dev/null;
fi

