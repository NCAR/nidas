#!/bin/sh

# Simple script for dumping A2D data.
# Alter to suit your needs.
#

usage() {
    echo "usage: $1 dsmid sensorid nchan sock/file [xml]"
    echo "  dsmid: integer dsm id from xml file"
    echo "  sensorid: integer A2D sensor id (typically 200)"
    echo "  nchan: how many configured A2D channels, 1-8"
    echo "  sock: just the string \"sock\" to read from sock:localhost:30000"
    echo "  file: a raw ads file to read"
    echo "  xml: optional name of XML config file"
    echo ""
    echo "Examples:"
    echo "  a2d_dump.sh 2 200 8 dsm_20060113_182338.ads"
    echo "	dsm=2, sensor 200, 8 channels, read from file"
    echo "	xml is in usual place in \$ADS3_CONFIG"
    echo ""
    echo "  a2d_dump.sh 2 200 2 sock"
    echo "	dsm=0, sensor 200, 2 channels, read from dsm_server socket"
    echo "	xml is in usual place in \$ADS3_CONFIG"
    exit 1
}

script=${0##*/}

xmlarg=

[ $# -ge 4 ] || usage $0

did=$1
sid=$2
nchan=$3
input=$4
[ $# -gt 4 ] && xmlarg="-x $5"

[ $input == "sock" ] && input = "sock:localhost:30000"

tmpawk=/tmp/${script}_$$.awk

trap "{ rm -f $tmpawk; }" EXIT

cat <<- 'EOD' > $tmpawk
BEGIN {
    nsamp = 5	# number of samples in a scan (5 for 500Hz data in 100Hz scans)
}
/^20/{
    f = 7
    for (i = 0; i < nsamp; i++) {
	printf("%d %d %d %s %d",$1,$2,$3,$4,$5);
        for (j = 0; j < nchan; j++) {
	    printf(" %d",$f)
	    f++
	}
	printf("\n")
    }
}
EOD

data_dump -d $did -s $sid -S $xmlarg $input | \
	awk -v nchan=$nchan -f $tmpawk
