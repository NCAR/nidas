#!/bin/bash

#Test script for DAUSensor, checking message reassembly and data processing.
echo "Beginning DAUSensor tests."

# straight up copied from ../sonic/run_test.sh

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

echo "data_dump executable: `which data_dump`"
echo "nidas libraries:"
ldd `which data_dump` | fgrep libnidas

compare() #based on compare() in tests/data_dump/runtest.sh
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
    echo "Comparing $reffile to $outfile"
    diff --side-by-side --width=200 --suppress-common-lines "$reffile" "$outfile"
    if [ $? -ne 0 ]; then
	echo "*** Output differs: $*"
	exit 1
    fi
    echo "Test successful."
}

#check that things are working w/o calibration. I have checked the sample file and know the message to processed message works correctly there.
compare data/sample_nocal.txt data_dump -p -H -i 1,20-21 -x config/nocalibration.xml data/testdata_20161121_00.dat


#check that setting the calibration to values other than slope 1 intercept 0 changes the output values appropriately.
compare data/sample_caltest.txt data_dump -p -H -i 1,20-21 -x config/calibrationtest.xml data/testdata_20161121_00.dat

#try valgrind.
valgrind_errors() {
    egrep -q "^==[0-9]*== ERROR SUMMARY:" $1 && \
        sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1 || echo 1
}

valgrind --leak-check=full data_dump -p -H -i 1,20-21 --logfields message -x config/nocalibration.xml data/testdata_20161121_00.dat > outputs/sample_nocal.txt 2> outputs/sample_nocal.txt.stderr

valgrind_errs=`valgrind_errors outputs/sample_nocal.txt.stderr`
if [ $valgrind_errs -eq 0 ]; then
    echo "Valgrind passed with $valgrind_errs errors."
else
    echo "Valgrind test failed with $valgrind_errs errors."
    exit 1
fi

#attempt to sanity check the output numbers (though don't have calibration data yet)
#temp: guessing -30<t<50 (celsius)
#humidity: 0<=h<=100 (percentage)
#pressure: 800<p<1100 (hpa, this is probably not a great guess
#radiation: ???
#rainfall: 0<r<??

check_rain(){
    if [ $1 -lt 0 ]; then
	echo "Failed value check: rain value of $1"
    fi
}

check_rh(){
    for var in $@; do
	if [ $var -lt 0 -o $var -gt 100 ]; then
	    echo "Failed value check: humidity value of $var"
	fi  
    done
}

check_temp(){
    for var in $@; do
	if [ $var -lt -30 -o $var -gt 50 ]; then
	    echo "Failed value check: temperature value of $var"
	fi
    done
}

check_pres(){
    if [ $1 -lt 900 -o $1 -gt 1100 ]; then
	echo "Failed value check: pressure value of $1"
    fi

}

check_rnet(){
    if [ $1 -lt 0 ]; then
	echo "Failed value check: net radiation value of $1"
    fi
}

check_line(){ #send appropriate numbers to their type of checker (based on unit)
    shift 6 #get rid of times, time elapsed, length
    check_rain $9
    #check_rh $1 $5
    #check_temp $2 $3 $4 $6
    #check_pres $7
    #check_rnet $8
}

data_dump -p -H -i 1,21 -x config/moxa.xml data/testdata_20161121_00.dat > outputs/check_val.txt 2>outputs/check_val.txt.stderr #generate file to read over for sanity checking.

egrep -v deltaT outputs/check_val.txt |
while read p; do
    check_line $p
done

echo "DAUSensor tests done."
exit 0
