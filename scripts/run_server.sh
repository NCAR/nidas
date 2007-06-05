#!/bin/sh

# if [ $# -gt 0 ]; then
#     export FLIGHT=$1
# fi

PATH=/opt/nidas/x86/bin:$PATH

echo "NIDAS_CONFIG=$NIDAS_CONFIG"
echo "FLIGHT=$FLIGHT"

if [ ! -f $NIDAS_CONFIG ]; then
    echo "$ads3dir does not exist"
    exit 1
fi

ulimit -c 1000000000

# status_listener > /dev/null 2>&1 &
# dsm_server | logger -p local5.notice  2>&1
#
# Remove any existing dsm_servers first.
pkill dsm_server
#
#valgrind --tool=memcheck --leak-check=yes dsm_server
dsm_server -d
