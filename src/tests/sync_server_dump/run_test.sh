#!/bin/bash

source ../nidas_tests.sh
check_executable sync_server
check_executable sync_dump

# Do a short test of sync_server, sync_dump.

export PROJ_DIR=$PWD/config

export FLIGHT=test123

find_tcp_port() {
    local -a inuse=(`netstat -tan | awk '/^tcp/{print $4}' | sed -r 's/.*:([0-9]+)$/\1/' | sort -u`)
    local port1=`cat /proc/sys/net/ipv4/ip_local_port_range | awk '{print $1}'`
    for (( port = $port1; ; port++)); do
        echo ${inuse[*]} | grep -F -q $port || break
    done
    echo $port
}

check_tcp_port() {
    local port=$1
    local -a inuse=(`netstat -tan | awk '/^tcp/{print $4}' | sed -r 's/.*:([0-9]+)$/\1/' | sort -u`)
    echo ${inuse[*]} | grep -F -q $port && echo "true"
    echo "false"
}

export SYNC_REC_PORT_TCP=`find_tcp_port`
echo "Using port=$SYNC_REC_PORT_TCP"

# To look at the latitude data
# data_dump -i 4,4072 -p data/dsm_20060908_200303.ads
 
echo "running sync_server in the background"
valgrind --leak-check=full --suppressions=suppressions.txt --gen-suppressions=all \
    sync_server -p $SYNC_REC_PORT_TCP \
    --log enable,level=verbose,function=SyncRecordSource::receive \
    --logfields level,message \
    --logparam trace_samples=4,4072 --log enable,level=info \
    data/dsm_20060908_200303.ads > sync_server.log 2>&1 &

echo "waiting for port $SYNC_REC_PORT_TCP to open, then run sync_dump"

for (( i=0; i<20; i++)); do
    `check_tcp_port $SYNC_REC_PORT_TCP` && break
    sleep 1
done

valgrind --leak-check=full --gen-suppressions=all sync_dump LAT_G sock:localhost:$SYNC_REC_PORT_TCP 2>&1 | \
    tee sync_dump.log

tmp1=$(mktemp /tmp/sync_rec_test_XXXXXX_expect.dat)
tmp2=$(mktemp /tmp/sync_rec_test_XXXXXX_actual.dat)

trap '{ rm -rf $tmp1 $tmp2; }' EXIT

cat << EOD > $tmp1
2006 09 08 20:03:03.000 nan
2006 09 08 20:03:04.000 nan
2006 09 08 20:03:05.000 nan
2006 09 08 20:03:06.484 39.9121
2006 09 08 20:03:07.376 39.9121
2006 09 08 20:03:08.000 nan
EOD

grep -E "^2006 09" sync_dump.log > $tmp2

dataok=true
if ! diff $tmp1 $tmp2; then
    echo "sync_dump data not as expected, files=/tmp/sync_rec_test*.dat"
    cp $tmp1 /tmp/sync_rec_test_expect.dat
    cp $tmp2 /tmp/sync_rec_test_actual.dat
    dataok=false
else
    echo "sync_dump data looks good"
fi

dump_errs=`valgrind_errors sync_dump.log`
echo "$dump_errs errors reported by valgrind in sync_dump.log"

sleep 5
server_errs=`valgrind_errors sync_server.log`
echo "$server_errs errors reported by valgrind in sync_server.log"

if $dataok && [ $dump_errs -eq 0 -a $server_errs -eq 0 ]; then
    exit 0
else
    exit 1
fi


