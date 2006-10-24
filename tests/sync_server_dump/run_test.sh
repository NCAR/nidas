#!/bin/sh

# Do a short test of sync_server, sync_dump.
# Not automated, one must read sync_server.log to
# look for issues.

export PROJ_DIR=$PWD/config
 
valgrind /opt/nidas/x86/bin/sync_server data/dsm_20060908_200303.ads \
    > sync_server.log 2>&1 &

sleep 10

valgrind /opt/nidas/x86/bin/sync_dump LAT_G sock:localhost:30001

