#!/bin/sh

# If the first runstring argument is "-i", then don't fiddle with PATH or
# LD_LIBRARY_PATH, and run the nidas programs from wherever they are found in PATH.
# Otherwise if build/apps is not found in PATH, prepend it, and if LD_LIBRARY_PATH 
# doesn't contain the string build, prepend ../build/{util,core,dynld}.

installed=false
[ $# -gt 0 -a "$1" == "-i" ] && installed=true

if ! $installed; then

    echo $PATH | fgrep -q build/apps || PATH=../../build/apps:$PATH

    llp=../../build/util:../../build/core:../../build/dynld
    echo $LD_LIBRARY_PATH | fgrep -q build || \
        export LD_LIBRARY_PATH=$llp${LD_LIBRARY_PATH:+":$LD_LIBRARY_PATH"}

    echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH
    echo PATH=$PATH

    if ! which dsm | fgrep -q build/; then
        echo "dsm program not found on build directory. PATH=$PATH"
        exit 1
    fi
    if ! ldd `which dsm` | awk '/libnidas/{if (index($0,"build/") == 0) exit 1}'; then
        echo "using nidas libraries from somewhere other than a build directory"
        exit 1
    fi
fi

# echo PATH=$PATH
# echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH

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
        
check_tcp_port() {
    local port=$1
    local -a inuse=(`netstat -tan | awk '/^tcp/{print $4}' | sed -r 's/.*:([0-9]+)$/\1/' | sort -u`)
    echo ${inuse[*]} | fgrep -q $port && echo "true"
    echo "false"
}
        
export SYNC_REC_PORT_TCP=`find_tcp_port`
echo "Using port=$SYNC_REC_PORT_TCP"

# To look at the latitude data
# data_dump -i 4,4072 -p data/dsm_20060908_200303.ads
 
echo "running sync_server in the background"
valgrind --leak-check=full --suppressions=suppressions.txt --gen-suppressions=all sync_server -p $SYNC_REC_PORT_TCP data/dsm_20060908_200303.ads \
    > sync_server.log 2>&1 &

echo "waiting for port $SYNC_REC_PORT_TCP to open, then run sync_dump"

for (( i=0; i<20; i++)); do
    `check_tcp_port $SYNC_REC_PORT_TCP` && break
    sleep 1
done

valgrind --leak-check=full --gen-suppressions=all sync_dump LAT_G sock:localhost:$SYNC_REC_PORT_TCP 2>&1 | \
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

dataok=true
if ! diff $tmp1 $tmp2; then
    echo "sync_dump data not as expected"
    rm -f $tmp1 $tmp2
    dataok=false
else
    echo "sync_dump data looks good"
fi


valgrind_errors() {
    sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1
}

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


