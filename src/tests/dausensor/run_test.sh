#!/bin/bash

#Test script for DAUSensor, checking message reassembly and data processing.
echo "Beginning DAUSensor tests."

source ../nidas_tests.sh

check_executable data_dump


#check that things are working w/o calibration. I have checked the sample file and know the message to processed message works correctly there.
compare data/sample_nocal.txt data_dump -p -H -i 1,20-21 -x config/nocalibration.xml data/testdata_20161121_00.dat


#check that setting the calibration to values other than slope 1 intercept 0 changes the output values appropriately.
compare data/sample_caltest.txt data_dump -p -H -i 1,20-21 -x config/calibrationtest.xml data/testdata_20161121_00.dat

#try valgrind.
valgrind_errors() {
    grep -E -q "^==[0-9]*== ERROR SUMMARY:" $1 && \
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

grep -E -v deltaT outputs/check_val.txt |
while read p; do
    check_line $p
done

echo "DAUSensor tests done."
exit 0
