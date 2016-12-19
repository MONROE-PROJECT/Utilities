#!/bin/bash

# Include the table of exit codes.
errcodes="/etc/watchdog.d/error-codes.inc"
[ -f "${errcodes}" ] && . $errcodes

bname=$(basename $0)

run_test ()
{
    IFS=' ' read -r -a lines <<< $(ps aux|grep autotunnel_watcher.sh|head -n -1|awk '{print $12}')
    for line in "${lines[@]}"
    do
        if [ $line = "/usr/bin/autotunnel_watcher.sh" ]
        then
            return $ENOERR
        fi
    done

    run_repair
    logger -t watchdog "${bname} ${1}: restarted autotunnel (exit ${?})"
    return $ENOERR
}


run_repair ()
{
    systemctl restart autotunnel
    local err=$?
    return $err
}


err=$ENOERR

# Process the command line.
case "${1}" in
  test)
    run_test
    err=$?
    ;;
  repair)
    run_repair "${2}" "${3}"
    err=$?
    ;;
  *)
    echo "Usage: ${bname} {test|repair errcode scriptname}"
    err=$EINVAL
esac

#logger -t watchdog "${bname} ${1} exiting with value ${err}"
exit $err
