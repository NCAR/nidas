#!/bin/sh

# Test script for a NIDAS iterators.

dir=`dirname $0`

PATH=$dir:$PATH

# If the first runstring argument is "installed", then don't fiddle with PATH or
# LD_LIBRARY_PATH, and run the nidas programs from wherever they are found in PATH.
# Otherwise if build_x86/build_apps is not found in PATH, prepend it, and if LD_LIBRARY_PATH 
# doesn't contain the string build_x86, prepend ../src/build_x86/build_{util,core,dynld}.

installed=false
[ $# -gt 0 -a "$1" == "-i" ] && installed=true

if ! $installed; then

    # echo $PATH | fgrep -q build_x86/build_apps || PATH=../../src/build_x86/build_apps:$PATH

    llp=../../src/build_x86/build_util:../../src/build_x86/build_core:../../src/build_x86/build_dynld
    echo $LD_LIBRARY_PATH | fgrep -q build_x86 || \
        export LD_LIBRARY_PATH=$llp${LD_LIBRARY_PATH:+":$LD_LIBRARY_PATH"}

    echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH
    echo PATH=$PATH

    if ! ldd `which ck_iterators` | awk '/libnidas/{if (index($0,"build_x86") == 0) exit 1}'; then
        echo "using nidas libraries from somewhere other than a build_x86 directory"
        exit 1
    fi
fi

echo PATH=$PATH
# echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH

echo "dsm executable: `which dsm`"
echo "nidas libaries:"
ldd `which dsm` | fgrep libnidas

valgrind_errors() {
    egrep -q "^==[0-9]*== ERROR SUMMARY:" $1 && \
        sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1 || echo 1
}

[ -d tmp ] || mkdir tmp

pwd
valgrind --suppressions=suppressions.txt --gen-suppressions=all ck_iterators iterator_test.xml 2>&1 | tee tmp/iter.log
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

