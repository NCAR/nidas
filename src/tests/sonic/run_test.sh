#!/bin/bash

# Script for testing the processing of sonic anemometer data.

echo "Starting sonic tests..."

source ../nidas_tests.sh
check_executable data_dump

data_dump="data_dump --precision 4 -l 6"

test_csat3() {
    export CSAT3_SHADOW_FACTOR=$1
    export CSAT3_ORIENTATION=$2
    export WIND3D_TILT_CORRECTION=$3
    export WIND3D_HORIZ_ROTATION=$4
    local compare_to=$5
    local msg="shadow=$1, orient=$2, tilt=$3, rotate=$4"
    local data_file=data/centnet_20120601_000000.dat.bz2
    echo "Testing CSAT3: $msg"
    compare $compare_to $data_dump -i 6,11 -p -x config/test.xml $data_file
}

export ATIK_SHADOW_FACTOR=0
export ATIK_SHADOW_ANGLE=70
export ATIK_ORIENTATION=normal

# "Truth" files were created with the data_dump program.
# They have not been verified otherwise, so this test is
# primarily a check that results don't change.

#          shadow orient   tilt  rotate truth-file
test_csat3 0.00 normal     false false no_cors.txt
test_csat3 0.00 normal     false true  horiz_rot.txt
test_csat3 0.00 normal     true  true  tilt_cor.txt
test_csat3 0.16 normal     true  true  shadow_cor.txt
test_csat3 0.16 normal     false false shadow_cor_only.txt
test_csat3 0.00 down       false false down.txt
test_csat3 0.00 flipped    false false flipped.txt
test_csat3 0.00 horizontal false false horizontal.txt
test_csat3 0.16 horizontal true  true  horizontal_all_cors.txt

test_csi_irga() {
    export CSAT3_SHADOW_FACTOR=$1
    export CSAT3_ORIENTATION=$2
    export WIND3D_TILT_CORRECTION=$3
    export WIND3D_HORIZ_ROTATION=$4
    local compare_to=$5
    local msg="shadow=$1, orient=$2, tilt=$3, rotate=$4"
    echo "Testing CSI_IRGA: $msg"
    local data_file=data/centnet_20151104_120000.dat.bz2 
    compare $compare_to $data_dump -i 1,41 -p -x config/test.xml $data_file
}

#             shadow orient    tilt  rotate truth-file
test_csi_irga 0.00 normal      false false csi_irga_no_cors.txt
test_csi_irga 0.00 normal      false true  csi_irga_horiz_rot.txt
test_csi_irga 0.00 normal      true  true  csi_irga_tilt_cor.txt
# shadow correction not supported yet for CSI_IRGA
test_csi_irga 0.16 normal      true  true  csi_irga_shadow_cor.txt
test_csi_irga 0.16 normal      false false csi_irga_shadow_cor_only.txt

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
    compare $compare_to $data_dump -i 6,81 -p -x config/test.xml $data_file
}

#          shadow orient   tilt  rotate truth-file
test_atik 0.00 normal     false false atik_no_cors.txt
test_atik 0.00 normal     false true  atik_horiz_rot.txt
test_atik 0.00 normal     true  true  atik_tilt_cor.txt
test_atik 0.16 normal     true  true  atik_shadow_cor.txt
test_atik 0.16 normal     false false atik_shadow_cor_only.txt
test_atik 0.00 flipped    false false atik_flipped.txt
test_atik 0.16 flipped    true  true  atik_flipped_all_cors.txt

echo "Sonic tests succeeded"
exit 0
