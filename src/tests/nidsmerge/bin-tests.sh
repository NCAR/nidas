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

clean() {
    mkdir -p outputs
    (set -x; rm -f outputs/*)
}

clean

run_merge() {
    # use --no-keep-opening since a file which cannot be opened is a problem
    if ! nidsmerge --log debug -r 300 --no-keep-opening "$@"; then
        failed "nidsmerge $*"
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

# Stats output on individual inputs should be the same as for merged
data_stats dsm12.dat dsm13.dat > outputs/baseline.stats.txt
data_stats outputs/dsm-both.dat > outputs/merged.stats.txt
run_diff outputs/baseline.stats.txt outputs/merged.stats.txt

# Run the merge including only one dsm at a time
run_merge -i dsm12.dat -i dsm13.dat --samples 12 -o outputs/dsm12.out.dat
run_merge -i dsm12.dat -i dsm13.dat --samples 13 -o outputs/dsm13.out.dat

# The merges should match the inputs
run_diff dsm12.dat outputs/dsm12.out.dat
run_diff dsm13.dat outputs/dsm13.out.dat

# in M2HATS excerpts, all t2 samples should also be in the network stream
run_merge -i isfs_20230731_0401.dat.bz2 -i t2_20230731_0401.dat.bz2 -o outputs/merged_20230731_0401.dat.bz2 || exit 1
data_stats t2_20230731_0401.dat.bz2 > outputs/t2_baseline.stats.txt
data_stats --samples 2 outputs/merged_20230731_0401.dat.bz2 > outputs/t2_merged.stats.txt
run_diff outputs/t2_baseline.stats.txt outputs/t2_merged.stats.txt

# the full stats in the network stream should match the merged stream, ie,
# everything in the t2 usb stream is already in the network stream
data_stats isfs_20230731_0401.dat.bz2 > outputs/m2hats_baseline.stats.txt
data_stats outputs/merged_20230731_0401.dat.bz2 > outputs/m2hats_merged.stats.txt
run_diff outputs/m2hats_baseline.stats.txt outputs/m2hats_merged.stats.txt

cat <<EOF
...Merge m2hats but exclude the t2 sonic from network stream by first creating
...a dat file with it explicitly excluded.
EOF

run_merge -s "2023-07-31_04:01" -e "2023-07-31_04:02" -i isfs_20230731_0401.dat.bz2 --samples ^2,10 -o outputs/isfs_20230731_0401_no_sonic.dat.bz2
echo ...make sure the sonic was excluded
nlines=`data_stats -i 2,10 outputs/isfs_20230731_0401_no_sonic.dat.bz2 2> /dev/null | tail -n +2 | wc -l`
if [ $nlines -ne 0 ]; then
    failed "sonic was not excluded from network stream"
fi
echo ...stats in the network stream without 2,10 should match the merged stream
data_stats isfs_20230731_0401.dat.bz2 --samples ^2,10 > outputs/isfs_no_sonic.stats.txt
data_stats outputs/isfs_20230731_0401_no_sonic.dat.bz2 > outputs/isfs_no_sonic_merged.stats.txt
run_diff outputs/isfs_no_sonic.stats.txt outputs/isfs_no_sonic_merged.stats.txt

# merging network and usb should still create the same merge as the baseline,
# except only if the start time is set explicitly, otherwise the start time is
# set to the first sample times in the files, which are unlikely to be the
# earliest times in the files.  for this case, setting the start time prevents
# 4 out-of-order samples from being dropped.
echo ...usb merged with no-sonic network stream should match full merge
run_merge -s "2023-07-31_04:01" -e "2023-07-31_04:02" -i t2_20230731_0401.dat.bz2 -i outputs/isfs_20230731_0401_no_sonic.dat.bz2 -o outputs/merged_20230731_0401_no_sonic.dat.bz2
data_stats outputs/merged_20230731_0401_no_sonic.dat.bz2 > outputs/m2hats_merged_no_sonic.stats.txt
run_diff outputs/m2hats_baseline.stats.txt outputs/m2hats_merged_no_sonic.stats.txt

# Now merging t2 without sonic while excluding sonic from network should be
# the same as excluding the sonic altogether.
#run_merge --samples ^2,10,file=isfs_, -i isfs_20230731_0401.dat.bz2 -i outputs/t2_20230731_0401_no_sonic.dat.bz2 -o outputs/merged_20230731_0401_no_sonic.dat.bz2


echo
echo "OK.  All tests passed."
