#!/bin/bash

# Test script which runs ck_xml on a bunch of project configurations.

# If the first runstring argument is "installed", then don't fiddle with PATH or
# LD_LIBRARY_PATH, and run the nidas programs from wherever they are found in PATH.
# Otherwise if build/apps is not found in PATH, prepend it, and if LD_LIBRARY_PATH 
# doesn't contain the string build, prepend ../build/{util,core,dynld}.

cwd=`dirname $0`
echo "cwd=$cwd"

installed=false
[ $# -gt 0 -a "$1" == "-i" ] && installed=true

if ! $installed; then

    echo $PATH | grep -F -q build/apps || PATH=../../build/apps:$PATH

    llp=../../build/util:../../build/core:../../build/dynld
    echo $LD_LIBRARY_PATH | grep -F -q build || \
        export LD_LIBRARY_PATH=$llp${LD_LIBRARY_PATH:+":$LD_LIBRARY_PATH"}

    echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH
    echo PATH=$PATH

    if ! which ck_xml | grep -F -q build/; then
        echo "ck_xml program not found on build directory. PATH=$PATH"
        exit 1
    fi
    if ! ldd `which ck_xml` | awk '/libnidas/{if (index($0,"build/") == 0) exit 1}'; then
        echo "using nidas libraries from somewhere other than a build directory"
        exit 1
    fi
fi

# echo PATH=$PATH
# echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH

echo "ck_xml executable: `which ck_xml`"
echo "nidas libaries:"
ldd `which ck_xml` | grep -F libnidas

valgrind_errors() {
    grep -E -q "^==[0-9]*== ERROR SUMMARY:" $1 && \
        sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1 || echo 1
}

[ -d $cwd/tmp ] && rm -rf $cwd/tmp
[ -d $cwd/tmp ] || mkdir $cwd/tmp

set -o pipefail

cp $cwd/../../xml/nidas.xsd $cwd/xml

for x in $cwd/xml/*.xml; do
    valgrind --suppressions=suppressions.txt --leak-check=full --gen-suppressions=all ck_xml $x 2>&1 1>/dev/null | tee tmp/ck_xml.log || exit 1

    # echo "PIPESTATUS[0]=${PIPESTATUS[0]}"
    # echo "PIPESTATUS[1]=${PIPESTATUS[1]}"

    # check for valgrind errors
    xml_errs=`valgrind_errors tmp/ck_xml.log`
    echo "$xml_errs errors reported by valgrind in tmp/ck_xml.log"

    if [ $xml_errs -ne 0 ]; then
        echo "ck_xml test failed"
        exit 1
    fi
done


