#! /bin/sh

export ISFF=$PWD/../prep/config
export PROJECT=TREX
export TREX_CONFIG=trex
export RAWDATADIR=$PWD/../prep/data

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

echo "dsm executable: `which dsm`"
echo "nidas libaries:"
ldd `which dsm` | fgrep libnidas


compare() # output command [...]
{
    reffile="$1"
    outfile=outputs/`basename "$reffile"`
    shift
    test -d outputs || mkdir outputs
    rm -f "$outfile"
    echo Running $*
    $* > "$outfile" 2> "${outfile}.stderr"
    if [ $? -ne 0 ]; then
	echo "*** Non-zero exit status: " $*
	cat "${outfile}.stderr"
	exit 1
    fi
    diff "$reffile" "$outfile"
    if [ $? -ne 0 ]; then
	echo "*** Output differs: " $*
	exit 1
    fi
}

datfile="$RAWDATADIR/projects/TREX/merge/isff_20060402_160000.dat"
xfile="../prep/config/projects/TREX/ISFF/config/trex.xml"

compare data_dump_-1,100.txt data_dump -i -1,100 $datfile
compare data_dump_-1,-1.txt data_dump -i -1,-1 $datfile
compare data_dump_1,0x32.txt data_dump -i 1,0x32 $datfile
compare data_dump_-1,0x32.txt data_dump -i -1,0x32 $datfile
compare data_dump_-p_-1,101.txt data_dump -p -x $xfile -i -1,101 $datfile
compare data_dump_-p_-1,101.txt data_dump -p -i -1,101 $datfile
compare data_dump_-p_-1,101_-1,51.txt data_dump -p -x $xfile \
    -i -1,101 -i -1,51 $datfile


