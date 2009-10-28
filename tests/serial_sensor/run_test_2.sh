#!/bin/sh

# Test script for a dsm and dsm_server process, sampling serial sensors, via pseudo-terminals

# If the first runstring argument is "installed", then don't fiddle with PATH or
# LD_LIBRARY_PATH, and run the nidas programs from wherever they are found in PATH.
# Otherwise if build_x86/build_apps is not found in PATH, prepend it, and if LD_LIBRARY_PATH 
# doesn't contain the string build_x86, prepend ../src/build_x86/build_{util,core,dynld}.

installed=false
[ $# -gt 0 -a "$1" == "-i" ] && installed=true

if ! $installed; then

    echo $PATH | fgrep -q build_x86/build_apps || PATH=../../src/build_x86/build_apps:$PATH

    llp=../../src/build_x86/build_util:../../src/build_x86/build_core:../../src/build_x86/build_dynld
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

find_udp_port() {
    local -a inuse=(`netstat -uan | awk '/^udp/{print $4}' | sed -r 's/.*:([0-9]+)$/\1/' | sort -u`)
    local port1=`cat /proc/sys/net/ipv4/ip_local_port_range | awk '{print $1}'`
    for (( port = $port1; ; port++)); do
        echo ${inuse[*]} | fgrep -q $port || break
    done
    echo $port
}
        
# kill any existing dsm processes
kill_dsm
kill_dsm_server

[ -d tmp ] && rm -rf tmp
[ -d tmp ] || mkdir tmp

# Start sensor simulations on pseudo-terminals.
# Once sensor_sim opens the pseudo-terminal it does a kill -STOP on itself.
# After starting the sensor_sims, this script then starts the dsm process.
# This script scans the dsm process output for an "opened tmp/testN" message
# indicating that the the dsm process has opened the device. At that point
# do a kill -CONT on the corresponding sensor_sim so it starts sending data
# on the pseudo terminal.
pids=()
sensor_sim -f data/test.dat -e "\n" -r 10 -t tmp/test0 &
pids=(${pids[*]} $!)
sensor_sim -f data/test.dat -b $'\e' -r 10 -t tmp/test1 &
pids=(${pids[*]} $!)
# simulate Campbell sonic
sensor_sim -c -r 60 -n 256 tmp/test2 -t &
pids=(${pids[*]} $!)
sensor_sim -f data/repeated_sep.dat -e xxy -r 1 -t tmp/test3 &
pids=(${pids[*]} $!)
sensor_sim -f data/repeated_sep.dat -b xxy -r 1 -t tmp/test4 &
pids=(${pids[*]} $!)
sensor_sim -f data/repeated_sep.dat -p "hello\n" -e "\n" -t tmp/test5 &
pids=(${pids[*]} $!)

# number of simulated sensors
nsensors=${#pids[*]}

export NIDAS_SVC_PORT_UDP=`find_udp_port`
echo "Using port=$NIDAS_SVC_PORT_UDP"

# ( valgrind dsm_server -d config/test.xml 2>&1 | tee tmp/dsm_server.log ) &
# valgrind dsm_server -d -l 6 config/test.xml > tmp/dsm_server.log 2>&1 &

export NIDAS_CONFIGS=config/configs.xml
# valgrind --tool=helgrind dsm_server -d -l 6 -r -c > tmp/dsm_server.log 2>&1 &
valgrind dsm_server -d -l 6 -r -c > tmp/dsm_server.log 2>&1 &

sleep 10

# start dsm data collection. Use udp port 30010 to contact dsm_server for XML
# ( valgrind dsm -d 2>&1 | tee tmp/dsm.log ) &
valgrind dsm -d -l 6 mcsock::$NIDAS_SVC_PORT_UDP > tmp/dsm.log 2>&1 &
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

# When a sensor_sim finishes and closes its pseudo-terminal
# the dsm process gets an I/O error reading the pseudo-terminal device.
# The dsm process then closes the device, and schedules it to be re-opened.
# Look for these "closed" messages in the dsm output, which indicate
# that the sensor_sim processes have finished.  We could instead
# check their process ids.
while true; do
    ndone=0
    for (( n = 0; n < $nsensors; n++ )); do
        if fgrep -q "closing: tmp/test$n" tmp/dsm.log; then
            ndone=$(($ndone + 1))
        else
            sleep 3
        fi
    done
    [ $ndone -eq $nsensors ] && break
done

kill_dsm
kill_dsm_server

# read archives from dsm and dsm_server process
for fp in localhost server; do

    # check output data file for the expected number of samples
    ofiles=(tmp/${fp}_*)
    if [ ${#ofiles[*]} -ne 1 ]; then
        echo "Expected one output file, got ${#ofiles[*]}"
        exit 1
    fi

    # run data_stats on raw data file
    statsf=tmp/data_stats_${fp}.out
    data_stats $ofiles > $statsf

    ns=`egrep "^localhost:tmp/test" $statsf | wc | awk '{print $1}'`
    if [ $ns -ne $nsensors ]; then
        echo "Expected $nsensors sensors in $statsf, got $ns"
        exit 1
    fi

    # should see these numbers of raw samples
    nsamps=(51 50 257 6 5 5)
    rawok=true
    for (( i = 0; i < $nsensors; i++)); do
        sname=test$i
        nsamp=${nsamps[$i]}
        awk -v nsamp=$nsamp "
    /^localhost:tmp\/$sname/{
        nmatch++
        if (\$4 != nsamp) {
            print \"sensor $sname, nsamps=\" \$4 \", should be \" nsamp
            exit(1)
        }
    }
    END{
        if (nmatch != 1) {
            print \"can't find sensor tmp/$sname in raw data_stats output\"
            exit(1)
        }
    }
    " $statsf || rawok=false
    done

    cat $statsf
    if ! $rawok; then
        echo "raw sample test failed"
    else
        echo "raw sample test OK"
    fi

    # run data through process methods
    procok=true
    data_stats -p $ofiles > $statsf

    ns=`egrep "^test1" $statsf | wc | awk '{print $1}'`
    if [ $ns -ne $nsensors ]; then
        echo "Expected $nsensors sensors in $statsf, got $ns"
        exit 1
    fi

    # should see these numbers of processed samples
    # The data file for the first 2 sensors has one bad record, so we
    # see one less processed sample.
    # The CSAT3 sonic sensor_sim sends out 1 query sample, and 256 data samples.
    # The process method discards first two samples so we see 254.

    nsamps=(50 49 254 5 4 5)
    for (( i = 0; i < $nsensors; i++)); do
        sname=test1.t$(($i + 1))
        nsamp=${nsamps[$i]}
        awk -v nsamp=$nsamp "
    /^$sname/{
        nmatch++
        if (\$4 != nsamp) {
            print \"sensor $sname, nsamps=\" \$4 \", should be \" nsamp
            exit(1)
        }
    }
    END{
        if (nmatch != 1) {
            print \"can't find variable $sname in processed data_stats output\"
            exit(1)
        }
    }
    " $statsf || procok=false
    done

    cat $statsf

done

# check for valgrind errors in dsm process
dsm_errs=`valgrind_errors tmp/dsm.log`
echo "$dsm_errs errors reported by valgrind in tmp/dsm.log"

# On shiraz, CentOS release 5.4 (Final), x86_64, these errors with cap_set_proc, cap_get_proc
# are reported by valgrind. Haven't been able to suppress them with memsets of arguments.
# --20716-- WARNING: unhandled syscall: 125
# --20716-- You may be able to write your own handler.
# --20716-- Read the file README_MISSING_SYSCALL_OR_IOCTL.
# --20716-- WARNING: unhandled syscall: 126
# --20716-- You may be able to write your own handler.
# --20716-- Read the file README_MISSING_SYSCALL_OR_IOCTL.
# 2009-10-28,11:34:45|WARNING|dsm_server: IOException: Process: cap_set_proc: Function not implemented
# --20716-- WARNING: unhandled syscall: 125
# --20716-- You may be able to write your own handler.
# --20716-- Read the file README_MISSING_SYSCALL_OR_IOCTL.
# ==20716== Conditional jump or move depends on uninitialised value(s)
# ==20716==    at 0x3FD3400FC5: cap_free (in /lib64/libcap.so.1.10)
# ==20716==    by 0x3FD34010DE: cap_get_proc (in /lib64/libcap.so.1.10)
# ==20716==    by 0x5395AD9: nidas::util::Process::getEffectiveCapability(int) (Process.cc:551)
# ==20716==    by 0x5068722: nidas::core::DSMServerApp::main(int, char**) (DSMServerApp.cc:208)
# ==20716==    by 0x40175A: main (dsm_server.cc:22)
# ==20716== 
# ==20716== Conditional jump or move depends on uninitialised value(s)
# ==20716==    at 0x3FD3401021: cap_free (in /lib64/libcap.so.1.10)
# ==20716==    by 0x3FD34010DE: cap_get_proc (in /lib64/libcap.so.1.10)
# ==20716==    by 0x5395AD9: nidas::util::Process::getEffectiveCapability(int) (Process.cc:551)
# ==20716==    by 0x5068722: nidas::core::DSMServerApp::main(int, char**) (DSMServerApp.cc:208)
# ==20716==    by 0x40175A: main (dsm_server.cc:22)

# ignore capget errors in valgrind.
ncap=`fgrep "Syscall param capget(data) points to unaddressable byte(s)" tmp/dsm.log | wc | awk '{print $1}'`
dsm_errs=$(($dsm_errs - $ncap))
ncap=`fgrep "cap_free (in /lib64/libcap" tmp/dsm.log | wc | awk '{print $1}'`
dsm_errs=$(($dsm_errs - $ncap))

# check for valgrind errors in dsm_server
svr_errs=`valgrind_errors tmp/dsm_server.log`
echo "$svr_errs errors reported by valgrind in tmp/dsm_server.log"
ncap=`fgrep "Syscall param capget(data) points to unaddressable byte(s)" tmp/dsm_server.log | wc | awk '{print $1}'`
svr_errs=$(($svr_errs - $ncap))
ncap=`fgrep "cap_free (in /lib64/libcap" tmp/dsm_server.log | wc | awk '{print $1}'`
svr_errs=$(($svr_errs - $ncap))

if [ $dsm_errs -eq 0 -a $svr_errs -eq 0 ]; then
    echo "serial_sensor test OK"
    exit 0
else
    echo "serial_sensor test failed"
    exit 1
fi

