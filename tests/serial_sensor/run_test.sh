#!/bin/sh

# Test script for a dsm process, sampling 3 serial sensors, via pseudo-terminals

valgrind_errors() {
    sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1
}

# kill any existing dsm processes
if pgrep -f "valgrind dsm" > /dev/null; then
    pkill -9 -f "valgrind dsm"
fi

echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH
echo PATH=$PATH

# build the local sensor_sim program
#cd src || exit 1
#scons
#cd ..

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

rm -f tmp/dsm.log

# start dsm data collection
( valgrind dsm -d config/test.xml 2>&1 | tee tmp/dsm.log ) &
dsmpid=$!

while ! [ -f tmp/dsm.log ]; do
    sleep 1
done

# look for "opened" messages in dsm output
ndone=0
sleep=0
sleepmax=20
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
    echo "serial_sensor test failed"
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
            sleep 1
        fi
    done
    [ $ndone -eq $nsensors ] && break
done

# send a TERM signal to dsm process
nkill=0
dsmpid=`pgrep -f "valgrind dsm"`
while ps -p $dsmpid > /dev/null; do
    if [ $nkill -gt 5 ]; then
        echo "Doing kill -9 $dsmpid"
        kill -9 $dsmpid
    else
        echo "Doing kill -TERM $dsmpid"
        kill -TERM $dsmpid
    fi
    nkill=$(($nkill + 1))
    sleep 10
done

# check output data file for the expected number of samples
ofiles=(tmp/localhost_*)
if [ ${#ofiles[*]} -ne 1 ]; then
    echo "Expected one output file, got ${#ofiles[*]}"
    exit 1
fi

# run data_stats on raw data file
statsf=tmp/data_stats.out
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
    if (\$4 != nsamp) {
        print \"sensor $sname, nsamps=\" \$4 \", should be \" nsamp
        exit(1)
    }
}
" $statsf || rawok=false
done

cat tmp/data_stats.out
if [ ! $rawok ]; then
    echo "raw sample test failed"
else
    echo "raw sample test OK"
fi

# run data through process methods
procok=true
statsf=tmp/data_stats.out
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
    sname=test$i
    nsamp=${nsamps[$i]}
    awk -v nsamp=$nsamp "
/^test:tmp\/$sname/{
    if (\$4 != nsamp) {
        print \"sensor $sname, nsamps=\" \$4 \", should be \" nsamp
        exit(1)
    }
}
" $statsf || procok=false
done

cat tmp/data_stats.out

# check for valgrind errors in dsm process
dump_errs=`valgrind_errors tmp/dsm.log`
echo "$dump_errs errors reported by valgrind in tmp/dsm.log"

# ignore capget error in valgrind.
fgrep -q "Syscall param capget(data) points to unaddressable byte(s)" tmp/dsm.log && dump_errs=$(($dump_errs - 1))

if $rawok && $procok && [ $dump_errs -eq 0 ]; then
    echo "serial_sensor test OK"
    exit 0
else
    echo "serial_sensor test failed"
    exit 1
fi

