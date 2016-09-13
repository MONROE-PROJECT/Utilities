#!/bin/sh

GREP=$(which grep);
PING=$(which ping);
TAIL=$(which tail);
REBOOT=$(which reboot);
DATE=$(which date);
DHCP_LEASES="/usr/zte/zte_conf/config/dnsmasq.leases";
HOST_IP="";
ICMP_LOST_COUNT=0;
#How many ICMP packets can be lost in a row. When the error this script is
#designed to detect occurs, we will loose everything
ICMP_LOST_LIMIT=5;

MISSING_HOST_TSTAMP=0
CUR_TSTAMP=0
#Which diffs should we consider (guard against clock adjustments)
TSTAMP_DIFF_THRESHOLD=3600
#If no host has been seen for 10 minutes, something is wrong. Worst-case,
#10 minutes should be enough for us to recover mifi in case script fails.
#And worst-worst case, we can do factory reset of mifi
MISSING_HOST_REBOOT_TIMEOUT=600
LOGFILE=/var/log/mf910-reboot.log

log()
{
    echo "[$($DATE +%s)] $1" >> $LOGFILE;
    # tiny logrotate
    $TAIL -n 100 $LOGFILE > $LOGFILE.tmp;
    mv $LOGFILE.tmp $LOGFILE;
}

handle_missing_host()
{
    local tstamp_diff=0;

    if [ $MISSING_HOST_TSTAMP -eq 0 ];
    then
        MISSING_HOST_TSTAMP=$($DATE +%s);
        log "Missing host tstamp $MISSING_HOST_TSTAMP";
        return;
    fi

    CUR_TSTAMP=$($DATE +%s);
    tstamp_diff=$(($CUR_TSTAMP - $MISSING_HOST_TSTAMP));

    if [ $tstamp_diff -lt $MISSING_HOST_REBOOT_TIMEOUT ];
    then
        return;
    fi

    #This will anyway be set in both cases. One, because we need a new time to
    #compare against if we have a large jump in time. Second, in case reboot
    #fails
    MISSING_HOST_TSTAMP=$CUR_TSTAMP;

    if [ $tstamp_diff -ge $TSTAMP_DIFF_THRESHOLD ];
    then
        log "Difference too great, keep checking";
        return;
    elif [ $tstamp_diff -ge $MISSING_HOST_REBOOT_TIMEOUT ];
    then
        log "No hosts seen for $MISSING_HOST_REBOOT_TIMEOUT sec, rebooting";
        "$REBOOT";
    fi
}

while true;
do
    HOST=$($GREP -m 1 "Monroe" $DHCP_LEASES);

    if [ -z "$HOST" ];
    then
        #Add timer + reboot here too
        log "No Monroe host found in DHCP leases";
        handle_missing_host
        sleep 5;
        continue;
    fi

    #There would not be an entry in dhcp leases without an IP, no need to
    #check for error
    HOST_IP=$(echo $HOST | awk '{print $3}');
    
    MISSING_HOST_TSTAMP=0;
    "$PING" -w 1 -c 1 "$HOST_IP";

    if [ $? -ne 0 ];
    then
        log "Could not reach host";
        ICMP_LOST_COUNT=$((ICMP_LOST_COUNT + 1));

        if [ $ICMP_LOST_COUNT -eq $ICMP_LOST_LIMIT ];
        then
            log "Will reboot mifi";
            ICMP_LOST_COUNT=0;
            "$REBOOT";
        fi

    else
        ICMP_LOST_COUNT=0;
    fi
 
    sleep 5;
done
