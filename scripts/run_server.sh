#!/bin/sh

if [ $# -gt 0 ]; then
    export ADS3_FLIGHT=$1
fi


PATH=/opt/ads3/x86/bin:$PATH

echo "ADS3_CONFIG=$ADS3_CONFIG"
echo "ADS3_PROJECT=$ADS3_PROJECT"
echo "ADS3_AIRCRAFT=$ADS3_AIRCRAFT"
echo "ADS3_FLIGHT=$ADS3_FLIGHT"

ads3dir=$ADS3_CONFIG/projects/$ADS3_PROJECT/$ADS3_AIRCRAFT/flights/$ADS3_FLIGHT

if [ ! -d $ads3dir ]; then
    echo "$ads3dir does not exist"
    exit 1
fi

ulimit -c 1000000000

# status_listener > /dev/null 2>&1 &
# dsm_server | logger -p local5.notice  2>&1
echo $ads3dir
#
# Remove any existing dsm_servers first.
pkill dsm_server
#
#valgrind --tool=memcheck --leak-check=yes dsm_server
dsm_server -d
