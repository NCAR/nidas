#!/bin/sh
##
## Shell script for generating Doxygen documents for the nidas code.
##
## Copyright 2005 UCAR, NCAR, All Rights Reserved
##
## $LastChangedDate$
##
## $LastChangedRevision$
##
## $LastChangedBy$
##
## $HeadURL$
##

nidas_dir() {
    if [ -z "$TMPDIR" ]; then
	echo /tmp/nidas_doxy
    else
	echo $TMPDIR/nidas_doxy
    fi
}

svn_co_nidas() {
    local dir=`nidas_dir`
    if [ -d $dir ]; then
	cd $dir
	svn update nidas
    else
	mkdir -p $dir || exit 1
	cd $dir || exit 1
	svn co http://svn/svn/nidas/trunk nidas
    fi
}

codir=`nidas_dir`/nidas
if [ ! -d $codir ]; then
    svn_co_nidas
else
    cd $codir || exit 1
    if  ! svn update; then
        svn cleanup || { cd $HOME; rm -rf $codir; svn_co_nidas; }
    fi
fi

cd $codir || exit 1

doxygen doc/doxygen_conf/nidas.doxy

rsync -a doc/doxygen /net/www/docs/isf/facilities/isff/nidas

