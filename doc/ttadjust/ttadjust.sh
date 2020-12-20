#!/bin/sh

usage() {
    echo "$0 outputdir"
    exit 1
}

if [ $# -lt 1 ]; then
    usage
fi

outdir=$1

if [ ${outdir:0:1} != / ]; then
    outdir=$(readlink -f $PWD/$outdir)
fi

cd $(dirname $0)

if false; then

export PROJ_DIR=/net/jlocal/projects

indir=/scr/raf_Raw_Data/WCR-TEST

infile=20201111_193000_tf01.ads

id=20,131
export DO_TTADJUST=1

logargs="--logconfig enable,level=verbose,function=TimetagAdjuster::adjust
--logparam trace_samples=$id --logconfig enable,level=info --logfields level,message"

if ! data_dump -i $id -p $logargs -x wcr-test.xml $indir/$infile \
	2> $outdir/psfd.err > $outdir/psfd.out ; then
    echo "data_dump failed, see $outdir/$psfd.err"
    exit 1
fi

# VERBOSE|HR [19,112]@2020 11 11 19:32:56.015, toff tdiff tdiffUncorr tdiffmin nDt: 7.00002 0 -5e-06 0 7
sed -r -n "/^VERBOSE|HR /s/[^@]+@([^,]+).*nDt:(.*)/\1 \2/p" $outdir/psfd.err > $outdir/psfd_adjust.out

fi

R BATCH --vanilla << EOD
source("ttadjust.R")
library("isfs")
psfd <- gethr(file.path("$outdir","psfd_adjust.out"))
pdf(file=file.path("$outdir", "psfd_ttadjust.pdf"))
plot_ttadj(psfd[1:1000,],nper=20)
dev.off()
q()
EOD

echo "status=$?"
