#!/bin/sh


# Test script for processing of sonic anemometer data.

echo "Starting sonic tests..."

# If the first runstring argument is "installed", then don't fiddle
# with PATH or LD_LIBRARY_PATH, and run the nidas programs from
# wherever they are found in PATH.
# Otherwise if build/apps is not found in PATH, prepend it, and if
# LD_LIBRARY_PATH doesn't contain the string build, prepend
# ../build/{util,core,dynld}.

installed=false
[ $# -gt 0 -a "$1" == "-i" ] && installed=true

if ! $installed; then

    echo $PATH | fgrep -q build/apps || PATH=../../build/apps:$PATH

    llp=../../build/util:../../build/core:../../build/dynld
    echo $LD_LIBRARY_PATH | fgrep -q build || \
        export LD_LIBRARY_PATH=$llp${LD_LIBRARY_PATH:+":$LD_LIBRARY_PATH"}

    echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH
    echo PATH=$PATH

    if ! which dsm | fgrep -q build/; then
        echo "dsm program not found on build directory. PATH=$PATH"
        exit 1
    fi
    if ! ldd `which dsm` | awk '/libnidas/{if (index($0,"build/") == 0) exit 1}'; then
        echo "using nidas libraries from somewhere other than a build directory"
        exit 1
    fi
fi

# echo PATH=$PATH
# echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH

echo "data_dump executable: `which data_dump`"
echo "nidas libaries:"
ldd `which data_dump` | fgrep libnidas

valgrind_errors() {
    egrep -q "^==[0-9]*== ERROR SUMMARY:" $1 && \
        sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1 || echo 1
}

tmperr=$(mktemp /tmp/sonic_test_XXXXXX)

tmp1=$(mktemp /tmp/sonic_test_XXXXXX)
tmp2=$(mktemp /tmp/sonic_test_XXXXXX)
tmp3=$(mktemp /tmp/sonic_test_XXXXXX)
tmp4=$(mktemp /tmp/sonic_test_XXXXXX)
trap "{ rm $f $tmp1 $tmp2 $tmp3 $tmp4 $tmperr; }" EXIT

# Output files to compare against.  
# These were created with data_dump at revision
# 863749e9bb6eba9bbcc58c87552f1f9bf0293db9
# They have not been verified otherwise, so this test
# is a check that results don't change.
out1=data/no_cors.txt.gz
out2=data/horiz_rot.txt.gz
out3=data/tilt_cor.txt.gz
out4=data/shadow_cor.txt.gz

error_exit() {
    cat $tmperr 1>&2
    exit 1
}

diff_exit() {
    echo "$1"
    cat $2
    # cat $4 | gzip -c > $3.gz
    exit 1
}

export WIND3D_HORIZ_ROTATION=false
export WIND3D_TILT_CORRECTION=false
export CSAT3_SHADOW_FACTOR=0
data_dump -l 6 -i 6,11 -p -x config/test.xml \
    data/centnet_20120601_000000.dat.bz 2> $tmperr > $tmp1 || error_exit
cat $tmperr

gunzip -c $out1 | diff -w - $tmp1 > $tmperr || diff_exit "Test failed: processed CSAT3 data differs" $tmperr $out1 $tmp1

export WIND3D_HORIZ_ROTATION=true
export WIND3D_TILT_CORRECTION=false
export CSAT3_SHADOW_FACTOR=0
data_dump -l 6 -i 6,11 -p -x config/test.xml \
    data/centnet_20120601_000000.dat.bz 2> $tmperr > $tmp2 || error_exit
cat $tmperr

gunzip -c $out2 | diff -w - $tmp2 > $tmperr || diff_exit "Test failed: rotated CSAT3 data differs" $tmperr $out2 $tmp2

export WIND3D_HORIZ_ROTATION=true
export WIND3D_TILT_CORRECTION=true
export CSAT3_SHADOW_FACTOR=0
data_dump -l 6 -i 6,11 -p -x config/test.xml \
    data/centnet_20120601_000000.dat.bz 2> $tmperr > $tmp3 || error_exit
cat $tmperr

gunzip -c $out3 | diff -w - $tmp3 > $tmperr || diff_exit "Test failed: tilt corrected CSAT3 data differs" $tmperr $out3 $tmp3

export WIND3D_HORIZ_ROTATION=true
export WIND3D_TILT_CORRECTION=true
export CSAT3_SHADOW_FACTOR=0.16
data_dump -l 6 -i 6,11 -p -x config/test.xml \
    data/centnet_20120601_000000.dat.bz 2> $tmperr > $tmp4 || error_exit
cat $tmperr

gunzip -c $out4 | diff -w - $tmp4 > $tmperr || diff_exit "Test failed: shadow corrected CSAT3 data differs" $tmperr $out4 $tmp4

echo "Sonic tests succeeded"
exit 0
