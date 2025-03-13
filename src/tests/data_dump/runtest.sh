#! /bin/bash

ulimit -c unlimited

export ISFF=$PWD/../prep/config
export PROJECT=TREX
export TREX_CONFIG=trex
export RAWDATADIR=$PWD/../prep/data

source ../nidas_tests.sh
check_executable data_dump

datfile="$RAWDATADIR/projects/TREX/merge/isff_20060402_160000.dat"
xfile="../prep/config/projects/TREX/ISFF/config/trex.xml"
data_dump="data_dump --precision 4"

compare data_dump_-1,100.txt ${data_dump} -i -1,100 $datfile
compare data_dump_-1,-1.txt ${data_dump} -i -1,-1 $datfile
compare data_dump_1,0x32.txt ${data_dump} -i 1,0x32 $datfile
compare data_dump_-1,0x32.txt ${data_dump} -i -1,0x32 $datfile
compare data_dump_-p_-1,101.txt ${data_dump} -p -x $xfile -i -1,101 $datfile
compare data_dump_-p_-1,101.txt ${data_dump} -p -i -1,101 $datfile
compare data_dump_-p_-1,101_-1,51.txt ${data_dump} -p -x $xfile \
    -i -1,101 -i -1,51 $datfile
compare data_dump_-p_-1,101.txt ${data_dump} -p -i '*,101' $datfile
compare data_dump_-1,-1.txt ${data_dump} -i '*,*' $datfile
