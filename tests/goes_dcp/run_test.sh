#!/bin/sh

# Test script for a dsm process, sampling serial sensors, via pseudo-terminals

# If the first runstring argument is "installed", then don't fiddle with PATH or
# LD_LIBRARY_PATH, and run the nidas programs from wherever they are found in PATH.
# Otherwise if build_x86/build_apps is not found in PATH, prepend it, and if LD_LIBRARY_PATH 
# doesn't contain the string build_x86, prepend ../src/build_x86/build_{util,core,dynld}.

installed=false
[ $# -gt 0 -a "$1" == "-i" ] && installed=true

if ! $installed; then

    echo $PATH | fgrep -q build_x86/build_apps/isff || PATH=../../src/build_x86/build_apps/isff:$PATH

    llp=../../src/build_x86/build_util:../../src/build_x86/build_core:../../src/build_x86/build_dynld
    echo $LD_LIBRARY_PATH | fgrep -q build_x86 || \
        export LD_LIBRARY_PATH=$llp${LD_LIBRARY_PATH:+":$LD_LIBRARY_PATH"}

    echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH
    echo PATH=$PATH

    if ! which pdecode | fgrep -q build_x86; then
        echo "pdecode program not found on build_x86 directory. PATH=$PATH"
        exit 1
    fi
    if ! ldd `which pdecode` | awk '/libnidas/{if (index($0,"build_x86") == 0) exit 1}'; then
        echo "using nidas libraries from somewhere other than a build_x86 directory"
        exit 1
    fi
fi

# echo PATH=$PATH
# echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH

echo "pdecode executable: `which pdecode`"
echo "nidas libaries:"
ldd `which pdecode` | fgrep libnidas

valgrind_errors() {
    egrep -q "^==[0-9]*== ERROR SUMMARY:" $1 && \
        sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1 || echo 1
}

[ -d tmp ] || mkdir tmp
rm -rf tmp/*

# decode GOES DCP file
valgrind --suppressions=suppressions.txt --gen-suppressions=all pdecode -l 6 -a -x isfs_tests.xml data/messages.010410.txt > tmp/pdecode.txt 2> tmp/pdecode.log || exit 1

# check for valgrind errors in pdecode process
errs=`valgrind_errors tmp/pdecode.log`
echo "$errs errors reported by valgrind in tmp/pdecode.log"

sed '1,/end header/d' tmp/pdecode.txt  > tmp/pdecode2.txt

if ! diff -q tmp/pdecode2.txt data/results.txt; then
    echo "pdecode results differ from expected"
    diff tmp/pdecode2.txt data/results.txt
    errs=1
fi

if [ $errs -eq 0 ]; then
    echo "goes_dcp test OK"
    exit 0
else
    echo "goes_dcp test failed"
    exit 1
fi

