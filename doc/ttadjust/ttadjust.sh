#!/bin/sh

usage() {
    echo "$0 var outputdir
var is PSFD or QCF"
    exit 1
}

if [ $# -lt 2 ]; then
    usage
fi

var=$1
case $var in
QCF)
    id=19,111
    ;;
PSFD)
    id=20,131
    ;;
*)
    usage
esac
    
outdir=$2

if [ ${outdir:0:1} != / ]; then
    outdir=$(readlink -f $PWD/$outdir)
fi

cd $(dirname $0)

outf=$outdir/${var}_ttadjust.out

do_data_dump=true

if $do_data_dump; then

    export PROJ_DIR=/net/jlocal/projects

    indir=/scr/raf_Raw_Data/WCR-TEST

    infile=20201111_193000_tf01.ads

    export DO_TTADJUST=1

    logargs="--logconfig enable,level=verbose,function=TimetagAdjuster::adjust
    --logparam trace_samples=$id --logconfig enable,level=info --logfields level,message"

    if ! data_dump -i $id -p $logargs -x wcr-test.xml $indir/$infile \
	    2> $outdir/$var.err > $outdir/$var.out ; then
	echo "data_dump failed, see $outdir/$var.err"
	exit 1
    fi

    # VERBOSE|HR [19,112]@2020 11 11 19:32:56.015, toff tdiff tdiffUncorr tdiffmin nDt: 7.00002 0 -5e-06 0 7
    sed -r -n "/^VERBOSE|HR /s/[^@]+@([^,]+).*nDt:(.*)/\1 \2/p" $outdir/$var.err > $outf

fi

opdf=$outdir/${var}_ttadjust.pdf

R BATCH --vanilla << EOD
source("ttadjust.R")
options(time.zone="UTC")
library("isfs")
dx <- gethr("$outf")

if ("$var" == "QCF") {
    # look at bad latency period for QCF
    t1 <- utime("2020 11 11 20:37:30")
    dx <- dx[utime(c(t1,t1+30)),]
} else {
    dx <- dx[1:1000,]
}

pdf(file="$opdf")
plot_ttadj(dx,nper=nper, title="WCR-TEST", var="$var")
dev.off()
q()
EOD

echo "status=$?"
