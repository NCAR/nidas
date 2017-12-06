#!/bin/sh

# Script for testing the processing of sonic anemometer data.

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

    if ! which data_dump | fgrep -q build/; then
        echo "dsm program not found on build directory. PATH=$PATH"
        exit 1
    fi
    if ! ldd `which data_dump` | awk '/libnidas/{if (index($0,"build/") == 0) exit 1}'; then
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
tmpout=$(mktemp /tmp/sonic_test_XXXXXX)
tmpout1=$(mktemp /tmp/sonic_test_XXXXXX)
tmpout2=$(mktemp /tmp/sonic_test_XXXXXX)
awkcom=$(mktemp /tmp/sonic_test_XXXXXX)
trap "{ rm $f $tmpout $tmperr $tmpout1 $tmpout2 $awkcom; }" EXIT

cat << \EOD > $awkcom
# BEGIN { CONVFMT="%.4g" }
/^2.*/{ 
    for (i = 1; i < 6 && i < NF; i++) {
        printf("%s ",$i)
    }
    for ( ; i < NF; i++) {
        n = $i
        printf("%.3g ",n)
    }
    printf("\n")
}
EOD

error_exit() {
    cat $tmperr 1>&2
    exit 1
}

diff_warn() {
    echo "$1" 1>&2
    cat $2 1>&2

    echo "Repeating diff with fewer significant digits"

    zcat $3 | awk -f $awkcom > $tmpout1
    cat $4 | awk -f $awkcom > $tmpout2

    if ! diff -w $tmpout1 $tmpout2; then
        echo "Second diff failed also"
        exit 1
    fi

    # Uncomment to create a "truth" file.
    # [ -f $3 ] || cat $4 | gzip -c > $3
}

diff_exit() {
    echo "$1" 1>&2
    cat $2 1>&2

    # Uncomment to create a "truth" file.
    # [ -f $3 ] || cat $4 | gzip -c > $3
    exit 1
}

test_csat3() {
    export CSAT3_SHADOW_FACTOR=$1
    export CSAT3_ORIENTATION=$2
    export WIND3D_TILT_CORRECTION=$3
    export WIND3D_HORIZ_ROTATION=$4
    local compare_to=$5
    local out=`basename "$compare_to" .gz`
    local msg="shadow=$1, orient=$2, tilt=$3, rotate=$4, output=$out"
    local data_file=data/centnet_20120601_000000.dat.bz2
    echo "Testing CSAT3: $msg"
    data_dump -l 7 -i 6,11 -p -x config/test.xml \
        $data_file 2> "${out}.log" > $out
    # cat $tmperr
    cp -fp "$out" "$tmpout"
    gunzip -c $compare_to | diff -w - $out > $tmperr || diff_warn "WARNING: differences in CSAT3 test of $msg, diff=" $tmperr $compare_to $tmpout
    echo "Test successful"
}

export ATIK_SHADOW_FACTOR=0
export ATIK_SHADOW_ANGLE=70
export ATIK_ORIENTATION=normal

# "Truth" files were created with the data_dump program.
# They have not been verified otherwise, so this test is
# primarily a check that results don't change.

#          shadow orient   tilt  rotate truth-file
test_csat3 0.00 normal     false false data/no_cors.txt.gz
test_csat3 0.00 normal     false true  data/horiz_rot.txt.gz
test_csat3 0.00 normal     true  true  data/tilt_cor.txt.gz
test_csat3 0.16 normal     true  true  data/shadow_cor.txt.gz
test_csat3 0.16 normal     false false data/shadow_cor_only.txt.gz
test_csat3 0.00 down       false false data/down.txt.gz
test_csat3 0.00 flipped    false false data/flipped.txt.gz
test_csat3 0.00 horizontal false false data/horizontal.txt.gz
test_csat3 0.16 horizontal true  true  data/horizontal_all_cors.txt.gz

test_csi_irga() {
    export CSAT3_SHADOW_FACTOR=$1
    export CSAT3_ORIENTATION=$2
    export WIND3D_TILT_CORRECTION=$3
    export WIND3D_HORIZ_ROTATION=$4
    local compare_to=$5
    local msg="shadow=$1, orient=$2, tilt=$3, rotate=$4"
    echo "Testing CSI_IRGA: $msg"
    local data_file=data/centnet_20151104_120000.dat.bz2 
    data_dump -l 6 -i 1,41 -p -x config/test.xml \
        $data_file 2> $tmperr > $tmpout || error_exit

    gunzip -c $compare_to | diff -w - $tmpout > $tmperr || diff_warn "WARNING: differences in CSI_IRGA test of $msg, diff=" $tmperr $compare_to $tmpout
    echo "Test successful"
}

#             shadow orient    tilt  rotate truth-file
test_csi_irga 0.00 normal      false false data/csi_irga_no_cors.txt.gz
test_csi_irga 0.00 normal      false true  data/csi_irga_horiz_rot.txt.gz
test_csi_irga 0.00 normal      true  true  data/csi_irga_tilt_cor.txt.gz
# shadow correction not supported yet for CSI_IRGA
test_csi_irga 0.16 normal      true  true  data/csi_irga_shadow_cor.txt.gz
test_csi_irga 0.16 normal      false false data/csi_irga_shadow_cor_only.txt.gz

test_atik() {
    export ATIK_SHADOW_FACTOR=$1
    export ATIK_SHADOW_ANGLE=70
    export ATIK_ORIENTATION=$2
    export WIND3D_TILT_CORRECTION=$3
    export WIND3D_HORIZ_ROTATION=$4
    local compare_to=$5
    local msg="shadow=$1, orient=$2, tilt=$3, rotate=$4"
    local data_file=data/centnet_20120601_000000.dat.bz2
    echo "Testing ATIK: $msg"
    data_dump -l 6 -i 6,81 -p -x config/test.xml \
        $data_file 2> $tmperr > $tmpout || error_exit

    gunzip -c $compare_to | diff -w - $tmpout > $tmperr || diff_warn "WARNING: differences in ATIK test of $msg, diff=" $tmperr $compare_to $tmpout
    echo "Test successful"
}

#          shadow orient   tilt  rotate truth-file
test_atik 0.00 normal     false false data/atik_no_cors.txt.gz
test_atik 0.00 normal     false true  data/atik_horiz_rot.txt.gz
test_atik 0.00 normal     true  true  data/atik_tilt_cor.txt.gz
test_atik 0.16 normal     true  true  data/atik_shadow_cor.txt.gz
test_atik 0.16 normal     false false data/atik_shadow_cor_only.txt.gz
test_atik 0.00 flipped    false false data/atik_flipped.txt.gz
test_atik 0.16 flipped    true  true  data/atik_flipped_all_cors.txt.gz

echo "Sonic tests succeeded"
exit 0
