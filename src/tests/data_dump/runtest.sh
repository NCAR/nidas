#! /bin/bash

ulimit -c unlimited

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

    if ! which data_dump | fgrep -q build/; then
        echo "data_dump program not found on build directory. PATH=$PATH"
        exit 1
    fi
    if ! ldd `which data_dump` | awk '/libnidas/{if (index($0,"build/") == 0) exit 1}'; then
        echo "using nidas libraries from somewhere other than a build directory"
        exit 1
    fi
fi

tmperr=$(mktemp /tmp/data_dump_XXXXXX)
awkcom=$(mktemp /tmp/data_dump_XXXXXX)
tmpout1=$(mktemp /tmp/data_dump_XXXXXX)
tmpout2=$(mktemp /tmp/data_dump_XXXXXX)
trap "{ rm $tmperr $awkcom $tmpout1 $tmpout2; }" EXIT

# echo PATH=$PATH
# echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH

echo "data_dump executable: `which data_dump`"
echo "nidas libaries:"
ldd `which data_dump` | fgrep libnidas

diff_warn() {
    echo $1
    cat $2 1>&2

    echo "Repeating diff with fewer significant digits"

    cat $3 | awk -f $awkcom > $tmpout1
    cat $4 | awk -f $awkcom > $tmpout2

    if ! diff -w $tmpout1 $tmpout2; then
        echo "Second diff failed also"
        exit 1
    fi
    echo "Second diff succeeded"

    # Uncomment to create a "truth" file.
    # [ -f $3 ] || cat $4 | gzip -c > $3
}

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

compare() # output command [...]
{
    reffile="$1"
    outfile=outputs/`basename "$reffile"`
    shift
    test -d outputs || mkdir outputs
    rm -f "$outfile"
    (set -x; "$@" > "$outfile" 2> "${outfile}.stderr")
    if [ $? -ne 0 ]; then
	echo "*** Non-zero exit status: $*"
	cat "${outfile}.stderr"
	exit 1
    fi
    diff --side-by-side --width=200 --suppress-common-lines "$reffile" "$outfile" > $tmperr || \
        diff_warn "WARNING: differences in data_dump test:" $tmperr $reffile $outfile
    if [ $? -ne 0 ]; then
        cat $tmperr
	echo "*** Output differs: $*"
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
compare data_dump_-p_-1,101.txt data_dump -p -i '*,101' $datfile
compare data_dump_-1,-1.txt data_dump -i '*,*' $datfile


