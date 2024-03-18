#!/bin/bash

# Test script for a dsm process, sampling serial sensors, via pseudo-terminals

# If the first runstring argument is "installed", then don't fiddle with PATH or
# LD_LIBRARY_PATH, and run the nidas programs from wherever they are found in PATH.
# Otherwise if build/apps is not found in PATH, prepend it, and if LD_LIBRARY_PATH 
# doesn't contain the string build, prepend ../build/{util,core,dynld}.

# scons may not set HOSTNAME
export HOSTNAME=`hostname`
vgopts="--suppressions=suppressions.txt --gen-suppressions=all --leak-check=full"
valgrind="valgrind $vgopts"

installed=false
debugging=false
while [ $# -gt 0 ]; do
    case "$1" in
        -i) installed=true;;
        -d) debugging=true ;;
    esac
    shift
done

if ! $installed; then

    echo $PATH | grep -F -q build/apps || PATH=../../build/apps:$PATH

    llp=../../build/util:../../build/core:../../build/dynld
    echo $LD_LIBRARY_PATH | grep -F -q build || \
        export LD_LIBRARY_PATH=$llp${LD_LIBRARY_PATH:+":$LD_LIBRARY_PATH"}

    echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH
    echo PATH=$PATH

    if ! which dsm | grep -F -q build/; then
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
ldd `which dsm` | grep -F libnidas

if $debugging; then
    export TEST=/tmp/test_debug
    mkdir $TEST
else
    export TEST=$(mktemp -d --tmpdir test_XXXXXX)
    trap "{ rm -rf $TEST; }" EXIT
fi
echo "TEST=$TEST"


valgrind_errors() {
    grep -E -q "^==[0-9]*== ERROR SUMMARY:" $1 && \
        sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1 || echo 1
}

start_dsm()
{
    export NIDAS_SVC_PORT_UDP=`find_udp_port`
    echo "Using port=$NIDAS_SVC_PORT_UDP"

    rm -f $TEST/dsm.log
    rm -f $TEST/${HOSTNAME}_*

    # start dsm data collection
    rm -f $TEST/dsm.pid
    ( $valgrind dsm -d --pid $TEST/dsm.pid -l 6 config/test.xml 2>&1 | tee $TEST/dsm.log ) &
    dsmpid=$!
}

kill_dsm() {
    echo "doing kill_dsm"
    # send a TERM signal to dsm process
    nkill=0
    dsmpid=`pgrep -f "valgrind.* dsm -d"`
    if [ -n "$dsmpid" ]; then
        echo "Doing kill -TERM $dsmpid"
        kill -TERM $dsmpid
        while ps -p $dsmpid > /dev/null; do
            if [ $nkill -gt 10 ]; then
                echo "Doing kill -9 $dsmpid"
                kill -9 $dsmpid
            fi
            nkill=$(($nkill + 1))
            sleep 1
        done
    fi
}

kill_sims() {
    for pid in ${sspids[*]}; do
        if [ $pid -gt 0 ] && kill -0 $pid >& /dev/null; then
            echo "Killing sensor_sim, pid=$pid"
            kill -9 $pid
        fi
    done
}


remove_devices() {
    for dev in $*; do
        rm -f $TEST/$dev
    done
}


wait_for_devices() {
    nsleeps=0
    for dev in $* ; do
        while [ ! -e "$TEST/$dev" ]; do
            if [ $nsleeps -ge 10 ]; then
                echo "Missing devices after 10 seconds!"
                exit 1
            fi
            echo "waiting for device $TEST/${dev}..."
            nsleeps=$(($nsleeps + 1))
            sleep 1
        done
    done
    echo "Devices found: $*"
}


find_udp_port() {
    which netstat >& /dev/null || { echo "netstat not found, install net-tools" && exit 1; }
    local -a inuse=(`netstat -uan | awk '/^udp/{print $4}' | sed -r 's/.*:([0-9]+)$/\1/' | sort -u`)
    local port1=$(( $(cat /proc/sys/net/ipv4/ip_local_port_range | awk '{print $1}') - 1))
    for (( port = $port1; ; port--)); do
        echo ${inuse[*]} | grep -F -q $port || break
    done
    echo $port
}


badexit() {
    if $debugging; then
        echo "See test debug directory: $TEST"
    else
        save=/tmp/test_save1
        echo "Saving $TEST as $save"
        [ -d $save ] && rm -rf $save
        mv $TEST $save
    fi
    exit 1
}


sspids=()

start_sim() # device [arg ...]
{
    device="$1"
    shift
    # default to waiting for HUP to clear on pts.  It can be overridden if -C
    # is passed to skip the wait, but really there's no reason for the
    # simulator not to wait for the dsm to open the pts.
    (set -x;
     exec sensor_sim -v -a 60 -H -t "$@" "$TEST/$device" > $TEST/${device}.log 2>&1) &
    pid=$!
    sspids=(${sspids[*]} $pid)
    nsensors=${#sspids[*]}
    echo started sensor $TEST/$device, pid $pid, nsensors=$nsensors
}


wait_on_sims()
{
    # technically this could be wait ${pids[*]}, but a loop might be useful to
    # diagnose failures
    for (( n = 0; n < $nsensors; n++ )); do
        echo "waiting on ${sspids[$n]} for $TEST/test$n"
        wait ${sspids[$n]}
    done
}


# Start sensor simulations on pseudo-terminals.

# Previously, this script scanned for "opening $TEST/testN" messages in the
# dsm.log to know when it was safe to signal the simulators to continue.
# However, the "opening" message is actually logged before the open() attempt,
# so there was a race condition between when the simulator started writing to
# the pty and when the dsm opened it.  This is now synchronized by using a new
# -H option to sensor_sim, so sensor_sim does not continue until the HUP is
# cleared, meaning the dsm has opened it's port.

# This synchronization ensures that the DSM does not write the sonic data mode
# prompts until sensor_sim is ready to read them, and sensor_sim does not
# start writing data messages until dsm is ready to read them.

# Since sensor_sim exits when it is done writing data, that is enough to
# synchronize when the dsm should be stopped.  Previously this script scanned
# for "closed" messages for each of the ports, but that could cause confusion
# when a sensor was closed because sensor_sim was not responding soon enough
# (ie, a sonic was not answering to data mode prompts and not reporting data).
# (It would be hard to guarantee that sensor_sim can respond like a hardware
# sensor, since it is subject to the process scheduler just like dsm.)

# It is safe to wait on the sensor_sim processes to finish, because they are
# guaranteed to exit on the alarm timeout.

# There is also no way to guarantee that all the sensors will even be opened
# at the same time.  It is possible (and has been observed) that some of the
# sensor_sim processes finish before the other ports have been opened.  Since
# port opens are handled by a different dsm thread and not synchronized in any
# way, the ordering depends entirely on the scheduler.  To synchronize the
# simulated sensors with each other, so their data are interleaved in the dsm
# reads, the sensor_sim processes would have to be synchronized somehow, such
# as by all running in one process.

kill_dsm

devices="test0 test1 test2 test3 test4 test5"
remove_devices $devices

# even though the sonic on test2 and prompted sensor on test5 will wait before
# sending data, they cannot start reading until the other pty is open, so they
# do not use -C either.
start_sim test0 -f data/test.dat -e "\n" -r 10
start_sim test1 -f data/test.dat -b $'\e' -r 10
# simulate Campbell sonic
start_sim test2 -c -r 60 -n 256
start_sim test3 -f data/repeated_sep.dat -e xxy -r 1
start_sim test4 -f data/repeated_sep.dat -b xxy -r 1
start_sim test5 -f data/repeated_sep.dat -p "hello\n" -e "\n"

# this makes sure the pseudo terminals have been created before starting dsm
wait_for_devices $devices
start_dsm
wait_on_sims
kill_dsm

# check output data file for the expected number of samples
ofiles=($TEST/${HOSTNAME}_*)
if [ ${#ofiles[*]} -ne 1 ]; then
    echo "Expected one output file, got ${#ofiles[*]}"
    badexit
fi

# run data_stats on raw data file
statsf=$TEST/data_stats.out
data_stats $ofiles > $statsf

ns=`grep -E "^$HOSTNAME:$TEST/test" $statsf | wc | awk '{print $1}'`
if [ $ns -ne $nsensors ]; then
    echo "Expected $nsensors sensors in $statsf, got $ns"
    badexit
fi

# should see these numbers of raw samples
nsamps=(53 52 257 6 5 5)
rawok=true
rawsampsok=true
for (( i = 0; i < $nsensors; i++)); do
    sname=test$i

    awk "
/^$HOSTNAME:\/.*\/$sname/{
    nmatch++
}
END{
    if (nmatch != 1) {
        print \"can't find sensor $TEST/$sname in raw data_stats output\"
        exit(1)
    }
}
" $statsf || rawok=false

    nsamp=${nsamps[$i]}
    awk -v nsamp=$nsamp "
/^$HOSTNAME:\/.*\/$sname/{
    nmatch++
    if (\$4 < nsamp) {
        print \"sensor $sname, nsamps=\" \$4 \", should be \" nsamp
        # if (\$4 < nsamp - 2) exit(1)
        exit(1)
    }
}
" $statsf || rawsampsok=false
done

cat $statsf
if ! $rawok; then
    echo "${0##*/}: raw sample test failed"
else
    echo "${0##*/}: raw sample test OK"
fi

# run data through process methods
statsf=$TEST/data_stats.out
$valgrind data_stats -l 6 -p $ofiles > $statsf

ns=`grep -E "^test1" $statsf | wc | awk '{print $1}'`
if [ $ns -ne $nsensors ]; then
    echo "Expected $nsensors sensors in $statsf, got $ns"
    badexit
fi

# should see these numbers of processed samples
# The data file for the first 2 sensors has one bad record, so we
# see one less processed sample.
# The CSAT3 sonic sensor_sim sends out 1 query sample, and 256 data samples.
# The process method discards first two samples so we see 254.

# we sometimes see less than the expected number of samples.
# Needs investigation.

nsamps=(52 50 254 5 4 5)
procok=true
procsampsok=true
for (( i = 0; i < $nsensors; i++)); do
    sname=test1.t$(($i + 1))
    awk "
/^$sname/{
    nmatch++
}
END{
    if (nmatch != 1) {
        print \"can't find variable $sname in processed data_stats output\"
        exit(1)
    }
}
" $statsf || procok=false

    nsamp=${nsamps[$i]}
    awk -v nsamp=$nsamp "
/^$sname/{
    nmatch++
    if (\$4 != nsamp) {
        print \"sensor $sname, nsamps=\" \$4 \", should be \" nsamp
        # if (\$4 < nsamp - 2) exit(1)
        exit(1)
    }
}
" $statsf || procsampsok=false
done

cat $statsf

# check for valgrind errors in dsm process
dsm_errs=`valgrind_errors $TEST/dsm.log`
echo "$dsm_errs errors reported by valgrind in $TEST/dsm.log"

echo "rawok=$rawok, procok=$procok, dsm_errs=$dsm_errs"

! $procok || ! $rawok && badexit

! $procsampsok || ! $rawsampsok && badexit

if [ $dsm_errs -eq 0 ]; then
    echo "${0##*/}: serial_sensor test OK"
    exit 0
else
    cat $TEST/dsm.log
    echo "${0##*/}: serial_sensor test failed"
    badexit
fi

