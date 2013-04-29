#!/bin/sh

# test script for bluetooth. This is not an integrated scons test
# since one must install the necessary bluetooth hardware on a
# system to do this test.

# If the first runstring argument is "installed", then don't fiddle with PATH or
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

echo "dsm executable: `which dsm`"
echo "nidas libaries:"
ldd `which dsm` | fgrep libnidas

valgrind_errors() {
    egrep -q "^==[0-9]*== ERROR SUMMARY:" $1 && \
        sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1 || echo 1
}

kill_dsm() {
    # send a TERM signal to dsm process
    nkill=0
    dsmpid=`pgrep -f "valgrind .*dsm -d"`
    if [ -n "$dsmpid" ]; then
        while ps -p $dsmpid > /dev/null; do
            if [ $nkill -gt 5 ]; then
                echo "Doing kill -9 $dsmpid"
                kill -9 $dsmpid
            else
                echo "Doing kill -TERM $dsmpid"
                kill -TERM $dsmpid
            fi
            nkill=$(($nkill + 1))
            sleep 5
        done
    fi
}

find_udp_port() {
    local -a inuse=(`netstat -uan | awk '/^udp/{print $4}' | sed -r 's/.*:([0-9]+)$/\1/' | sort -u`)
    local port1=`cat /proc/sys/net/ipv4/ip_local_port_range | awk '{print $1}'`
    for (( port = $port1; ; port++)); do
        echo ${inuse[*]} | fgrep -q $port || break
    done
    echo $port
}
        
kill_dsm

for f in /tmp/dsm.pid; do
    if [ -f $f ]; then
        echo "$f exists, deleting"
        rm $f || exit 1
    fi
done

[ -d tmp ] && rm -rf tmp
[ -d tmp ] || mkdir tmp

rm -f tmp/dsm.log

# start dsm data collection
valgrind --suppressions=suppressions.txt --gen-suppressions=all dsm -d -l 6 config/test.xml

