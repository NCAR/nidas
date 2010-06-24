#!/bin/sh

# Test script for a dsm and dsm_server process, sampling serial sensors, via pseudo-terminals

# If the first runstring argument is "installed", then don't fiddle with PATH or
# LD_LIBRARY_PATH, and run the nidas programs from wherever they are found in PATH.
# Otherwise if build_x86/build_apps is not found in PATH, prepend it, and if LD_LIBRARY_PATH 
# doesn't contain the string build_x86, prepend ../build_x86/build_{util,core,dynld}.

installed=false
[ $# -gt 0 -a "$1" == "-i" ] && installed=true

if ! $installed; then

    echo $PATH | fgrep -q build_x86/build_apps || PATH=../../../build_x86/build_apps:$PATH

    llp=../../../build_x86/build_util:../../../build_x86/build_core:../../../build_x86/build_dynld
    echo $LD_LIBRARY_PATH | fgrep -q build_x86 || \
        export LD_LIBRARY_PATH=$llp${LD_LIBRARY_PATH:+":$LD_LIBRARY_PATH"}

    # echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH
    # echo PATH=$PATH

    if ! which dsm | fgrep -q build_x86; then
        echo "dsm program not found on build_x86 directory. PATH=$PATH"
        exit 1
    fi
    if ! ldd `which dsm` | awk '/libnidas/{if (index($0,"build_x86") == 0) exit 1}'; then
        echo "using nidas libraries from somewhere other than a build_x86 directory"
        exit 1
    fi
fi

# echo PATH=$PATH
# echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH

echo "dsm executable: `which dsm`"
echo "nidas libaries:"
ldd `which dsm` | fgrep libnidas

valgrind_errors() {
    sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1
}

kill_dsm() {
    # send a TERM signal to dsm process
    nkill=0
    dsmpid=`pgrep -f "valgrind dsm -d"`
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

kill_dsm_server() {
    # send a TERM signal to dsm_server process
    nkill=0
    dsmpid=`pgrep -f "valgrind dsm_server"`
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

# kill any existing dsm processes
kill_dsm
kill_dsm_server

[ -d tmp ] && rm -rf tmp
[ -d tmp ] || mkdir tmp

pkill -TERM sensor_sim

# Start sensor simulations on pseudo-terminals.
# Once sensor_sim opens the pseudo-terminal it does a kill -STOP on itself.
# After starting the sensor_sims, this script then starts the dsm process.
# This script scans the dsm process output for an "opened tmp/testN" message
# indicating that the the dsm process has opened the device. At that point
# do a kill -CONT on the corresponding sensor_sim so it starts sending data
# on the pseudo terminal.
pids=()
sensor_sim -F data/test.dat -e "\n" -r 1 -t tmp/test0 &
pids=(${pids[*]} $!)
sensor_sim -F data/test.dat -e "\n" -r 1 -t tmp/test1 &
pids=(${pids[*]} $!)

# number of simulated sensors
nsensors=${#pids[*]}

# ( valgrind dsm_server -d config/test.xml 2>&1 | tee tmp/dsm_server.log ) &
valgrind dsm_server -d config/test_server.xml > tmp/dsm_server.log 2>&1 &

sleep 10

# start dsm data collection. Use port 30010 to contact dsm_server for XML
# ( valgrind dsm -d 2>&1 | tee tmp/dsm.log ) &
valgrind dsm -d mcsock::30010 > tmp/dsm.log 2>&1 &
dsmpid=$!

while ! [ -f tmp/dsm.log ]; do
    sleep 1
done

# look for "opened" messages in dsm output
ndone=0
sleep=0
sleepmax=40
while [ $ndone -lt $nsensors -a $sleep -lt $sleepmax ]; do
    for (( n = 0; n < $nsensors; n++ )); do
        if [ ${pids[$n]} -gt 0 ]; then
            if fgrep -q "opening: tmp/test$n" tmp/dsm.log; then
                echo "sending CONT to ${pids[$n]}"
                kill -CONT ${pids[$n]}
                pids[$n]=-1
                ndone=$(($ndone + 1))
            else
                sleep 1
                sleep=$(($sleep + 1))
            fi
        fi
    done
done

if [ $sleep -ge $sleepmax ]; then
    echo "Cannot find \"opened\" messages in dsm output."
    echo "dsm process is apparently not running successfully."
    echo "Perhaps a firewall is blocking the configuration multicast?"
    echo "serial_sensor test failed"
    kill_dsm
    kill_dsm_server
    exit 1
fi

