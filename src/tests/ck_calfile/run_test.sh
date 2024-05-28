#!/bin/bash

# Test script which runs ck_calfile on some sample cal files and compares
# results against previous runs

cwd=`dirname $0`
# echo "cwd=$cwd"

source ../nidas_tests.sh
check_executable ck_calfile

set -o pipefail

tmpout=$(mktemp /tmp/ck_calfile_XXXXXX.out)
tmperr=$(mktemp /tmp/ck_calfile_XXXXXX.err)
trap '{ rm -f $tmpout $tmperr; }' EXIT

for cf in $cwd/caldir1/*.dat; do
    f=${cf##*/}
    cmd="ck_calfile caldir1:caldir2 $f"
    if ! valgrind --suppressions=suppressions.txt --leak-check=full --gen-suppressions=all $cmd 2>$tmperr 1>$tmpout; then
        echo "'$cmd' failed: $(cat $tmperr)"
        exit 1
    fi

    # echo "PIPESTATUS[0]=${PIPESTATUS[0]}"
    # echo "PIPESTATUS[1]=${PIPESTATUS[1]}"

    # check for valgrind errors
    xml_errs=`valgrind_errors $tmperr`
    echo "$xml_errs errors reported by valgrind for command: $cmd"

    if [ $xml_errs -ne 0 ]; then
        save=/tmp/ck_calfile_$$.err
        cp $tmperr $save
        echo "test of '$cmd' failed, stderr copied to $save"
        exit 1
    fi

    checkfile=ck_calfile_results/${f%.dat}.out

    if ! diff -u $tmpout $checkfile > $tmperr; then
        echo "Output of '$cmd' differs from what's expected in $checkfile"
        saveout=/tmp/ck_calfile_$$.out
        cp $tmpout $saveout
        save=/tmp/ck_calfile_$$.diff
        diff -u $saveout $checkfile > $save
        echo "test of '$cmd' failed, result copied to $saveout, diff copied to $save"
        echo "If differences look like time zone offsets, check that tzdata package is installed"
        exit 1
    fi

done
