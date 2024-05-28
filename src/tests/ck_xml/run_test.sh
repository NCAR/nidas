#!/bin/bash

# Test script which runs ck_xml on a bunch of project configurations.

cwd=`dirname $0`
echo "cwd=$cwd"

source ../nidas_tests.sh

check_executable ck_xml

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


