#!/bin/sh

# Do a short test of sync_server, sync_dump.

export PROJ_DIR=$PWD/config
 
echo "running sync_server in the background"
valgrind /opt/nidas/x86/bin/sync_server data/dsm_20060908_200303.ads \
    > sync_server.log 2>&1 &

echo "sleeping, then run sync_dump"
sleep 10

valgrind /opt/nidas/x86/bin/sync_dump LAT_G sock:localhost:30001 2>&1 | \
    tee sync_dump.log

tmp1=/tmp/$0.$$.expect
tmp2=/tmp/$0.$$.actual

cat << EOD > $tmp1
2006 09 08 20:03:03.000 nan
2006 09 08 20:03:04.000 nan
2006 09 08 20:03:05.000 nan
2006 09 08 20:03:06.484 39.9121
EOD

egrep "^2006 09" sync_dump.log > $tmp2

if ! diff $tmp1 $tmp2; then
    echo "sync_dump data not as expected"
else
    echo "sync_dump data looks good"
fi

rm -f $tmp1 $tmp2

valgrind_errors() {
    sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1
}

server_errors=`valgrind_errors sync_server.log`
echo "$server_errors errors reported by valgrind in sync_server.log"

dump_errs=`valgrind_errors sync_dump.log`
echo "$dump_errs errors reported by valgrind in sync_dump.log"



