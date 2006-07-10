#!/bin/sh
##
## Shell script for generating Doxygen documents for the nidas code.
##
## Copyright 2005 UCAR, NCAR, All Rights Reserved
##
## $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $
##
## $LastChangedRevision: 1703 $
##
## $LastChangedBy: wasinger $
##
## $HeadURL: http://orion/svn/hiaper/nidas/dsm/doxy/make_doc $
##

nidas_dir() {
    # echo /scr/tmp/$LOGNAME/nidas_doxy
    echo /tmp/nidas_doxy
}

svn_co_nidas() {
    local dir=`nidas_dir`
    [ -d $dir ] && rm -rf $dir
    mkdir -p $dir
    cd $dir
    svn co http://svn/svn/nids/branches/nidas_reorg nidas
    cd nidas
}

codir=`nidas_dir`/nidas
if [ ! -d $codir ]; then
    svn_co_nidas
else
    cd $codir || exit 1
    svn update || svn_co_nidas
fi

cd $codir || exit 1

doxygen doc/doxygen_conf/nidas.doxy

rsync -a doc/doxygen /net/www/docs/isf/facilities/isff/nidas

