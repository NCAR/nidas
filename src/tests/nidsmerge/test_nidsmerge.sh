#!/bin/bash

# Run some basic tests on nidsmerge

# The raw data file excerpts were created from M2HATS data as follows:
#
# nidsmerge --samples 2-10,/ -i ~/Data/M2HATS/isfs/raw_data/t2_20230731_000000.dat -s "2023-07-31T04:01" -e "2023-07-31T04:02" -o t2_20230731_0401.dat.bz2
# nidsmerge --samples 2-10,/ -i ~/Data/M2HATS/isfs/raw_data/isfs_20230731_040000.dat.bz2 -s "2023-07-31T04:01" -e "2023-07-31T04:02" -o isfs_20230731_0401.dat.bz2
#

dir=`dirname $0`

PATH=$dir:$PATH

source ../nidas_tests.sh
check_executable nidsmerge
check_executable data_stats
check_executable data_dump
check_executable sensor_extract

options="--track-origins=no --num-callers=30 --show-below-main=no"
options="$options --error-exitcode=1"
options="$options --errors-for-leak-kinds=definite"
memcheck="--tool=memcheck --leak-check=full"
valgrind="valgrind $memcheck $options"

if [ "$1" = "--no-valgrind" ]; then
    valgrind=""
    shift
fi


clean() {
    mkdir -p outputs
    (set -x; rm -f outputs/*)
}

clean

run_merge() {
    # use --no-keep-opening since a file which cannot be opened is a problem
    logging="--logfields level,message --log verbose,file=nidsmerge.cc"
    echo nidsmerge $logging -r 300 --no-keep-opening "$@"
    if ! $valgrind nidsmerge $logging -r 300 --no-keep-opening "$@"; then
        failed "nidsmerge $*"
    fi
}

run_sensor_extract() {
    logging="--logfields level,message --log info"
    echo sensor_extract $logging "$@"
    if ! $valgrind sensor_extract $logging "$@"; then
        failed "sensor_extract $*"
    fi
}

run_diff()
{
    if ! diff "$@"; then
        failed "diff $*"
    fi
}

# Merge some simple sample files
run_merge -i dsm12.dat -i dsm13.dat -o outputs/dsm-both.dat

echo Stats output on individual inputs should be the same as for merged...
data_stats dsm12.dat dsm13.dat > outputs/baseline.stats.txt
data_stats outputs/dsm-both.dat > outputs/merged.stats.txt
run_diff outputs/baseline.stats.txt outputs/merged.stats.txt

echo Run the merge including only one dsm at a time...
run_merge -i dsm12.dat -i dsm13.dat --samples 12 -o outputs/dsm12.out.dat
run_merge -i dsm12.dat -i dsm13.dat --samples 13 -o outputs/dsm13.out.dat

echo The merges should match the inputs...
run_diff dsm12.dat outputs/dsm12.out.dat
run_diff dsm13.dat outputs/dsm13.out.dat

echo M2HATS excerpts: all t2 samples should also be in the network stream...
run_merge -i isfs_20230731_0401.dat.bz2 -i t2_20230731_0401.dat.bz2 -o outputs/merged_20230731_0401.dat.bz2 || exit 1
data_stats t2_20230731_0401.dat.bz2 > outputs/t2_baseline.stats.txt
data_stats --samples 2 outputs/merged_20230731_0401.dat.bz2 > outputs/t2_merged.stats.txt
run_diff outputs/t2_baseline.stats.txt outputs/t2_merged.stats.txt

# everything in the t2 usb stream is already in the network stream
echo Network stream stats should match the merged stream...
data_stats isfs_20230731_0401.dat.bz2 > outputs/m2hats_baseline.stats.txt
data_stats outputs/merged_20230731_0401.dat.bz2 > outputs/m2hats_merged.stats.txt
run_diff outputs/m2hats_baseline.stats.txt outputs/m2hats_merged.stats.txt

cat <<EOF
...Merge m2hats but exclude the t2 sonic from network stream by first creating
...a dat file with it explicitly excluded.
EOF

times="-s 2023-07-31_04:01 -e 2023-07-31_04:02"
run_merge $times -i t2_20230731_0401.dat.bz2 --samples ^2,10 -o outputs/t2_20230731_0401_no_sonic.dat.bz2
echo ...make sure the sonic was excluded
nlines=`data_stats -i 2,10 outputs/t2_20230731_0401_no_sonic.dat.bz2 2> /dev/null | tail -n +2 | wc -l`
if [ $nlines -ne 0 ]; then
    failed "sonic was not excluded from usb stream"
fi
echo ...stats in the network stream without 2,10 should match the merged stream
data_stats t2_20230731_0401.dat.bz2 --samples ^2,10 > outputs/t2_no_sonic.stats.txt
data_stats outputs/t2_20230731_0401_no_sonic.dat.bz2 > outputs/t2_no_sonic_merged.stats.txt
run_diff outputs/t2_no_sonic.stats.txt outputs/t2_no_sonic_merged.stats.txt

# merging network and usb should still create the same merge as the baseline,
# except only if the start time is set explicitly, otherwise the start time is
# set to the first sample times in the files, which are unlikely to be the
# earliest times in the files.  for this case, setting the start time prevents
# 4 out-of-order samples from being dropped.
echo ...no-sonic usb merged with network stream should match full merge
run_merge $times -i outputs/t2_20230731_0401_no_sonic.dat.bz2 -i isfs_20230731_0401.dat.bz2 -o outputs/merged_20230731_0401_no_usb_sonic.dat.bz2
data_stats outputs/merged_20230731_0401_no_usb_sonic.dat.bz2 > outputs/m2hats_merged_no_usb_sonic.stats.txt
run_diff outputs/m2hats_baseline.stats.txt outputs/m2hats_merged_no_usb_sonic.stats.txt

echo ...no-sonic usb merged with network stream without start time
testout=merged_20230731_0401_no_usb_sonic_no_start
run_merge -i outputs/t2_20230731_0401_no_sonic.dat.bz2 -i isfs_20230731_0401.dat.bz2 -o outputs/${testout}.dat.bz2
data_stats outputs/${testout}.dat.bz2 > outputs/${testout}.stats.txt
run_diff outputs/m2hats_baseline.stats.txt outputs/${testout}.stats.txt

echo ...no-sonic usb merged with network stream without start time, 10 sec readahead
testout=merged_20230731_0401_no_usb_sonic_no_start_10_secs
run_merge -r 10 -i outputs/t2_20230731_0401_no_sonic.dat.bz2 -i isfs_20230731_0401.dat.bz2 -o outputs/${testout}.dat.bz2
data_stats outputs/${testout}.dat.bz2 > outputs/${testout}.stats.txt
run_diff outputs/m2hats_baseline.stats.txt outputs/${testout}.stats.txt

# Now merging t2 and isfs while excluding sonic from network should be the
# same as merging with the network sonic already excluded.  since the samples
# in t2_ are explicitly included, the rest of the samples in isfs_ files have
# to be explicitly included also.  the "normal" way to run this would be to
# use just the single exclude.
echo ...merge t2 and network while excluding sonic from network
run_merge $times --samples ^2,10,file=isfs_ --samples /,file=isfs_ --samples /,file=t2_ -i isfs_20230731_0401.dat.bz2 -i t2_20230731_0401.dat.bz2 -o outputs/merged_20230731_0401_filter_network_sonic.dat.bz2
data_stats outputs/merged_20230731_0401_filter_network_sonic.dat.bz2 > outputs/m2hats_merged_filter_network_sonic.stats.txt
run_diff outputs/m2hats_baseline.stats.txt outputs/m2hats_merged_filter_network_sonic.stats.txt

# Two samples in the output will be adjusted forward to offsets of 1 and 2
# microseconds.
testout=channel2_20230920_005950_merged
dumpfile=${testout}.dump.txt
echo ...merge single file with non-increasing times and non-char samples
run_merge --force-increasing-times -i channel2_20230920_005950.dat -o outputs/${testout}.dat >& outputs/${testout}.log
data_dump -i /,501 -i /,512 --timeformat "%Y-%m-%d_%H:%M:%S.%6f" outputs/${testout}.dat > outputs/$dumpfile
run_diff baseline/$dumpfile outputs/$dumpfile
if ! grep "Warning: Found 6 samples with non-increasing times!" outputs/${testout}.log > /dev/null; then
    failed "expected warning about non-increasing time tags"
fi
if ! grep "Sample times were adjusted to force increasing times." outputs/${testout}.log > /dev/null; then
    failed "expected warning about non-increasing time tags"
fi

# Same operation but without forcing increasing times, there should still be a
# warning about non-increasing times.
testout=channel2_20230920_005950_merged_noninc
dumpfile=${testout}.dump.txt
echo ...merge single file with non-increasing times, check warning
run_merge -i channel2_20230920_005950.dat -o outputs/${testout}.dat >& outputs/${testout}.log
# dump should show all the samples sorted in time order, with 2 extra samples
# at :03 and :04 seconds.  there should be 6 non-increasing time tags.
data_dump -i /,501 -i /,512 --timeformat "%Y-%m-%d_%H:%M:%S.%6f" outputs/${testout}.dat > outputs/$dumpfile
run_diff baseline/$dumpfile outputs/$dumpfile
if ! grep "Warning: Found 6 samples with non-increasing times!" outputs/${testout}.log > /dev/null; then
    failed "expected warning about non-increasing time tags"
fi

echo ...sensor_extract DSM 8 and reassign to 9
testout=extract_8_to_9
run_sensor_extract isfs_20230731_0401.dat.bz2  -o outputs/${testout}.dat --samples 8=9
data_stats outputs/${testout}.dat > outputs/${testout}.dump.txt
run_diff baseline/${testout}.dump.txt outputs/${testout}.dump.txt

echo
echo "OK.  All tests passed."
