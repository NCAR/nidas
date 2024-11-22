#!/bin/sh
##
## Shell script for generating Doxygen documents for the nidas code.
##
## Copyright 2005 UCAR, NCAR, All Rights Reserved
##

htmldest=jenkins@jenkins.eol.ucar.edu:/net/www/docs/software/nidas/doxygen/html

if [ ! -f doc/doxygen_conf/nidas.doxy ]; then
    echo "doc/doxygen_conf/nidas.doxy not found."
    echo "Run this script from the top of the NIDAS source tree."
    exit 1
fi

make_html()
{
    set -x
    rm -rf doc/doxygen/html
    scons -C src dox
}

push_eol()
{
    if [ ! -f doc/doxygen/html/index.html ]; then
        echo "Doxygen index.html not found.  Run make_html first."
        exit 1
    fi
    (set -x; rsync -a -O --no-perms --delete doc/doxygen/html/ $htmldest)
}

case "$1" in
    push*)
        push_eol
        ;;
    make*)
        make_html
        ;;
    *)
        echo "Usage: $0 [make|push]"
        ;;
esac
