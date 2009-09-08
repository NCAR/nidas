#!/bin/sh

llp=../../src/build_x86/build_util:../../src/build_x86/build_core:../../src/build_x86/build_dynld
[ -n "$LD_LIBARARY_PATH" ] || export LD_LIBRARY_PATH=$llp

PATH=../../src/build_x86/build_apps:$PATH

# Do a short test of sync_server, sync_dump.

export PROJ_DIR=$PWD/config

export FLIGHT=test123

find_tcp_port() {
    local -a inuse=(`netstat -tan | awk '/^tcp/{print $4}' | sed -r 's/.*:([0-9]+)$/\1/' | sort -u`)
    local port1=`cat /proc/sys/net/ipv4/ip_local_port_range | awk '{print $1}'`
    for (( port = $port1; ; port++)); do
        echo ${inuse[*]} | fgrep -q $port || break
    done
    echo $port
}
        
export SYNC_REC_PORT_TCP=`find_tcp_port`
echo "Using port=$SYNC_REC_PORT_TCP"

# To look at the latitude data
# data_dump -i 4,4072 -p data/dsm_20060908_200303.ads
 
echo "running sync_server in the background"
valgrind /opt/local/nidas/x86/bin/sync_server -d data/dsm_20060908_200303.ads \
    > sync_server.log 2>&1 &

echo "sleeping, then run sync_dump"
sleep 15

valgrind /opt/local/nidas/x86/bin/sync_dump LAT_G sock:localhost:30001 2>&1 | \
    tee sync_dump.log

tmp1=/tmp/$0.$$.expect
tmp2=/tmp/$0.$$.actual

cat << EOD > $tmp1
2006 09 08 20:03:03.000 nan
2006 09 08 20:03:04.000 nan
2006 09 08 20:03:05.000 nan
2006 09 08 20:03:06.484 39.9121
2006 09 08 20:03:07.376 39.9121
EOD

egrep "^2006 09" sync_dump.log > $tmp2

if ! diff $tmp1 $tmp2; then
    echo "sync_dump data not as expected"
    rm -f $tmp1 $tmp2
else
    echo "sync_dump data looks good"
fi


valgrind_errors() {
    sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1
}

dump_errs=`valgrind_errors sync_dump.log`
echo "$dump_errs errors reported by valgrind in sync_dump.log"

sleep 5
server_errors=`valgrind_errors sync_server.log`
echo "$server_errors errors reported by valgrind in sync_server.log"



