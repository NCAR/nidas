#! /bin/bash

ulimit -c unlimited

source ../nidas_tests.sh
check_executable data_stats
check_executable datasets

datfile="marshall_ttt_20250527_120000_30s.dat"
xmldir="xml"
xfile="$xmldir/marshall2023.xml"
data_stats="data_stats -p -a -D --period 15 -n 1 --precision 4"
data_stats="$data_stats --xml $xfile $datfile"

export CALFILES="xml/cal_files"
if [ -f "$xmldir/datasets.xml" ]; then
    eval `datasets "$xmldir/datasets.xml" -b noqc_geo`
fi

mkdir -p outputs
$data_stats --json outputs/data_stats.json > outputs/stdout.json || exit 1
compare_outputs baseline/data_stats.json outputs/data_stats.json
compare_outputs baseline/stdout.json outputs/stdout.json

# check basic data_stats without xml or processing options
data_stats $datfile > outputs/raw.txt || exit 1
compare_outputs baseline/raw.txt outputs/raw.txt
