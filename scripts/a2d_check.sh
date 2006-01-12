#!/bin/sh

# Simple for detecting spikes in raw counts from the A2D.
# Alter to suit your needs.
# Setup:
# 1. a reference voltage into A2D - make sure it is within the A2D
#    voltage limits.
# 2. start dsm_server
# 3. start dsm
# 4. run this script on the same host as is running dsm_server
#	The data_dump connection is to localhost (could be changed).
# Examples:
#	dsm=2, sensor 200, channel 0, outlier delta of 7 counts
#	a2d_check.sh 2 200 0 7 xml/ads3.xml
#
#	dsm=0, sensor 200, channel 4, outlier delta of 2 counts
#	   xml is in usual place in $ADS3_CONFIG
#	a2d_check.sh 2 200 4 2
#

usage() {
    echo "usage: $1 dsmid sensorid chan delta [xml]"
    echo "dsmid: integer dsm id from xml file"
    echo "sensorid: integer A2D sensor id (typically 200)"
    echo "chan: which A2D channel to monitor: 0-N (where N is number of config'd channels"
    echo "delta: if counts varies by delta from an initial average then complain"
    echo "xml: optional name of XML config file"
    exit 1
}

script=${0##*/}

xmlarg=

[ $# -ge 3 ] || usage $0

did=$1
sid=$2
chan=$3
delta=$4
[ $# -gt 4 ] && xmlarg="-x $5"

tmpawk=/tmp/${script}_$$.awk

trap "{ rm -f $tmpawk; }" EXIT

cat <<- 'EOD' > $tmpawk
BEGIN {
    cnt = 0
    cavg = 0	
}
/^20/{
    f = 7 + chan
    if (cnt < 10) {
	# average first 10 counts
	for (i = 0; i < 5; i++) {
	    cavg += $f
	    f += 8
	    cnt++
	}
	if (cnt == 10) cavg /= cnt
    }
    else {
	for (i = 0; i < 5; i++) {
	    c = $f
	    # report variation more than 3 counts from first count
	    cd = c - cavg
	    if (cd > delta || cd < -delta)
	    	print "outlier: ",$1,$2,$3,$4,"val=",c,"avg=",cavg,"diff=",cd
	    f += 8
	}
    }
}
EOD

data_dump -d $did -s $sid -S $xmlarg sock:localhost:30000 | awk -v chan=$chan -v delta=$delta -f $tmpawk
