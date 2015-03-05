#!/bin/sh

# Test script which runs ck_calfile on some sample cal files and compares
# results against previous runs

# If the first runstring argument is "installed", then don't fiddle with PATH or
# LD_LIBRARY_PATH, and run the nidas programs from wherever they are found in PATH.
# Otherwise if build/apps is not found in PATH, prepend it, and if LD_LIBRARY_PATH 
# doesn't contain the string build, prepend ../build/{util,core,dynld}.

cwd=`dirname $0`
# echo "cwd=$cwd"

installed=false
[ $# -gt 0 -a "$1" == "-i" ] && installed=true

if ! $installed; then

    echo $PATH | fgrep -q build/apps || PATH=../../build/apps:$PATH

    llp=../../build/util:../../build/core:../../build/dynld
    echo $LD_LIBRARY_PATH | fgrep -q build || \
        export LD_LIBRARY_PATH=$llp${LD_LIBRARY_PATH:+":$LD_LIBRARY_PATH"}

    echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH
    echo PATH=$PATH

    if ! which ck_calfile | fgrep -q build/; then
        echo "ck_calfile program not found on build directory. PATH=$PATH"
        exit 1
    fi
    if ! ldd `which ck_calfile` | awk '/libnidas/{if (index($0,"build/") == 0) exit 1}'; then
        echo "using nidas libraries from somewhere other than a build directory"
        exit 1
    fi
fi

# echo PATH=$PATH
# echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH

echo "ck_calfile executable: `which ck_calfile`"
echo "nidas libaries:"
ldd `which ck_calfile` | fgrep libnidas

valgrind_errors() {
    egrep -q "^==[0-9]*== ERROR SUMMARY:" $1 && \
        sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1 || echo 1
}

set -o pipefail

tmpout=$(mktemp /tmp/ck_calfile_XXXXXX.out)
tmperr=$(mktemp /tmp/ck_calfile_XXXXXX.err)
trap '{ rm -f $tmpout $tmperr; }' EXIT

for cf in $cwd/caldir1/*.dat; do
    f=${cf##*/}
    cmd="ck_calfile caldir1:caldir2 $f"
    valgrind --suppressions=suppressions.txt --leak-check=full --gen-suppressions=all $cmd 2>$tmperr 1>$tmpout || exit 1

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
        exit 1
    fi

done


