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

codir=${TMPDIR:-/tmp}/nidas_doxy/nidas

svn_co_nidas() {
    if [ -d $codir ]; then
	cd $codir || exit 1
        if  ! svn update; then
            svn cleanup
            svn update || { cd $HOME; rm -rf $codir || exit 1; svn_co_nidas; }
        fi
    else
        [ -d ${codir%/*} ] || mkdir -p ${codir%/*}
	cd ${codir%/*} || exit 1
	svn co http://svn/svn/nidas/trunk nidas
    fi
}

svn_co_nidas
cd $codir || exit 1

doxygen doc/doxygen_conf/nidas.doxy

rsync -a doc/doxygen /net/www/docs/isf/facilities/isff/nidas

