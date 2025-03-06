#!/bin/bash

# Test script for nidas::util::UTime

dir=`dirname $0`

PATH=$dir:$PATH

source ../nidas_tests.sh
check_executable ck_utime

[ -d tmp ] || mkdir tmp

# pipefail: status returned is value of rightmost command to exit with non-zero status
set -o pipefail

ck_utime="ck_utime --log_level=all"
ck_utime="ck_utime"
valgrind --suppressions=suppressions.txt --leak-check=full --gen-suppressions=all $ck_utime 2>&1 | tee tmp/utime.log
stat=$?

errs=`valgrind_errors tmp/utime.log`
echo "$errs errors reported by valgrind in tmp/utime.log"

if [ $stat -eq 0 -a $errs -eq 0 ]; then
    echo "UTime test OK"
else
    echo "UTime test failed"
    exit 1
fi


# Test utime program with various runstring arguments
export TZ=MST7MDT
# which utime

# UTC input, UTC output
args=(1970 jan 01 00:00 +"%Y-%m-%d %H:%M:%S %Z")
str=`utime "${args[@]}"`
expect="1970-01-01 00:00:00 GMT"
if [ "$str" != "$expect" ]; then
    echo "utime ${args[@]} returned \"$str\", expected: \"$expect\""
    exit 1
fi

# UTC input, local time output
args=(-L 1970 jan 01 00:00 +"%Y-%m-%d %H:%M:%S %Z")
str=`utime "${args[@]}"`
expect="1969-12-31 17:00:00 MST"
if [ "$str" != "$expect" ]; then
    echo "utime ${args[@]} returned \"$str\", expected: \"$expect\""
    exit 1
fi

# local time input, UTC output
args=(-l 1970 jan 01 00:00 +"%Y-%m-%d %H:%M:%S %Z")
str=`utime "${args[@]}"`
expect="1970-01-01 07:00:00 GMT"
if [ "$str" != "$expect" ]; then
    echo "utime ${args[@]} returned \"$str\", expected: \"$expect\""
    exit 1
fi

# local time input, local time output
args=(-l -L 1970 jan 01 00:00 +"%Y-%m-%d %H:%M:%S %Z")
str=`utime "${args[@]}"`
expect="1970-01-01 00:00:00 MST"
if [ "$str" != "$expect" ]; then
    echo "utime ${args[@]} returned \"$str\", expected: \"$expect\""
    exit 1
fi

