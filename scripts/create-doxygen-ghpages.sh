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

workingdir=${TMPDIR:-/tmp}/nidas_ghpages


git_clone_nidas() { #checkoug && cleanup
    #-depth=1 only pulls current changes since documentation is a snapshot in time
    if [ -d $workingdir ]; then
        #Bail if somehow the repo isnt linear.  Needs human intervention
        pushd $workingdir
        git pull --ff-only --progress --depth=1 -f  || exit 1 
        popd
    else 
        mkdir -p $workingdir
        git clone --progress --depth=1 https://github.com/ncareol/nidas.git $workingdir
    fi

    rm -fr $workingdir/doc/doxygen || exit 1 #wipe out any old cruft
    mkdir -p $workingdir/doc/doxygen || exit 1 #re-create new folder
}

create_ghpages() {
    pushd $workingdir || exit 1 #in the git root
    echo "Running Doxygen.  Might want to get some coffee"
    doxygen doc/doxygen_conf/nidas.doxy 2>1 1>doxygen.log || exit 1 #get a coffee
    pushd doc/doxygen/html
    git init .
    git checkout --orphan gh-pages
    git add -A *
    git commit -m "documentation-for-gh-pages"
    git remote add origin git@github.com:ncareol/nidas.git
    git push -u -f origin gh-pages
    popd # doc/doxygen
    popd # $workingdir
}

git_clone_nidas
create_ghpages
