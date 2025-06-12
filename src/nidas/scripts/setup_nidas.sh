#! /bin/bash
#
# Since this script can be sourced or executed, it cannot call exit or return.
# Instead it runs all the way through.

setup_nidas_usage() {
    echo "$0 [options] [set|unset] [<nidas-install-path>]"
    cat <<EOF

Given a NIDAS install path, adjust PATH and LD_LIBRARY_PATH to refer to that
install.  This script can be sourced to change the values of the current
shell.  It also echoes the new settings, so it can also be evaluated.

  set     Add the given NIDAS directory to the front of the paths.
  unset   Remove the NIDAS directory from the paths.
  --sh    Echo sh commands for eval.
  --csh   Echo csh commands for eval.

If nidas-install-path not specified, then it is derived from the location
of this script.  If neither set nor unset specified, then set is assumed.
EOF
    setup_nidas_error=1
}

# BASH_SOURCE works even when the script is sourced, unlike $0.  And do not use
# realpath in case the canonical path has links.
thispath=`dirname "${BASH_SOURCE[0]}"`
topdir=$(cd "${thispath}/.." && pwd)
setup_nidas_error=0

setup_nidas_eval=""
setup_nidas_op="set"
while [ $# -gt 0 ]; do

    case "$1" in

        set|unset)
            setup_nidas_op="$1"
            shift
            ;;

        --help|-h)
            setup_nidas_usage
            break
            ;;

        --sh)
            setup_nidas_eval="sh"
            shift
            ;;

        --csh)
            setup_nidas_eval="csh"
            shift
            ;;

        -*)
            echo "unrecognized option: $1"
            setup_nidas_usage
            break
            ;;

        *)
            topdir="$1"
            shift
            ;;

    esac

done

# Sanity check
if [ $setup_nidas_error -eq 0 -a -z "$topdir" -o -z "$setup_nidas_op" ]; then
    setup_nidas_usage
fi

# Remove any element of epath starting with edir.
remove() # epath edir
{
    epath="$1"
    edir="$2"
    # Remove this path if already there.
    removed=`echo ":$epath" | sed -E -e "s,:${edir}[^:]*,,g"`
    # Then remove leading and trailing colons.
    removed=`echo "$removed" | sed -E -e "s,^:+,," -e "s,:+\$,,"`
    echo "$removed"
}

prepend() # epath edir
{
    epath="$1"
    edir="$2"
    colon=":"
    [ -z "$epath" ] && colon=""
    echo "${edir}${colon}${epath}"
}

setup_nidas_unset() # topdir
{
    if [ -n "$1" ]; then
        topdir="$1"
    elif [ -n "$NIDAS_TOPDIR" ]; then
        topdir="$NIDAS_TOPDIR"
    else
        echo "usage: setup_nidas_unset {topdir}"
        return
    fi
    # Make sure to include a trailing slash since these must match the full top
    # directory.
    ldpath=`remove "$LD_LIBRARY_PATH" "$topdir/"`
    epath=`remove "$PATH" "$topdir/"`
    export LD_LIBRARY_PATH="$ldpath"
    export PATH="$epath"
    unset NIDAS_TOPDIR
}

setup_nidas_set() # topdir
{
    if [ -z "$1" ]; then
        echo "usage: setup_nidas_set {topdir}"
        return
    fi
    topdir="$1"
    bindir="$topdir/bin"
    # library subdirectory should now be the same on all platforms
    libdir="$topdir/lib"
    # Avoid adding a directory which might be malformed if
    # it doesn't look like a nidas install.
    if [ -x "$bindir/dsm" -a -x "$libdir/." ]; then
        setup_nidas_unset "$topdir"
        ldpath=`prepend "$LD_LIBRARY_PATH" "$libdir"`
        epath=`prepend "$PATH" "$bindir"`
        export LD_LIBRARY_PATH="$ldpath"
        export PATH="$epath"
        # stash the current topdir, as a default for unset.
        export NIDAS_TOPDIR="$topdir"
    else
        setup_nidas_error=1
    fi
}

# If this is sourced, then the functions are available as aliases.
alias snset=setup_nidas_set
alias snunset=setup_nidas_unset

if [ $setup_nidas_error -eq 0 ]; then

    case "$setup_nidas_op" in

        set)
            setup_nidas_set "$topdir"
            ;;

        unset)
            setup_nidas_unset "$topdir"
            ;;

    esac

fi

if [ $setup_nidas_error -eq 0 ]; then

    case "$setup_nidas_eval" in

        sh)
            echo export PATH=\'"${PATH}"\' ';'
            echo export LD_LIBRARY_PATH=\'"${LD_LIBRARY_PATH}"\'
            echo export NIDAS_TOPDIR=\'"${NIDAS_TOPDIR}"\'
            ;;
        csh)
            echo setenv PATH \'"${PATH}"\' ';'
            echo setenv LD_LIBRARY_PATH \'"${LD_LIBRARY_PATH}"\'
            echo setenv NIDAS_TOPDIR \'"${NIDAS_TOPDIR}"\'
            ;;

    esac

fi
