#!/bin/bash

# Test script for a NIDAS iterators.

dir=`dirname $0`

PATH=$dir:$PATH

source ../nidas_tests.sh
check_executable ck_iterators

[ -d tmp ] || mkdir tmp

valgrind --suppressions=suppressions.txt --gen-suppressions=all ck_iterators iterator_test.xml > tmp/iter.log 2>&1
stat=$?

errs=`valgrind_errors tmp/iter.log`
echo "$errs errors reported by valgrind in tmp/iter.log"

if [ $stat -eq 0 -a $errs -eq 0 ]; then
    echo "iterator test OK"
    exit 0
else
    echo "iterator test failed"
    exit 1
fi
