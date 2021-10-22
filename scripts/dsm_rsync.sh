#!/bin/bash

TEMP=$(getopt --long -o "PhIna:" "$@")

if [ $? -ne 0 ]; then
    exit 1
fi

eval set -- "$TEMP"
unset TEMP

function showHelp()
{
    cat <<EOF
Script to rsync the cross compiled ARM binaries to a DSM.

usage: ./dsm_rsync [options] <source> <target>

 -a <armhf|armel> set the architecture instead of detecting it

Options -P, -n, -I are passed directly to rsync:

 -P  show progress
 -I  ignore times and copy everything over
 -n  dryn run

The <source> is the top directory of a nidas installation, such as
/opt/nidas.  The script detects the target architecture from the file
<source>/bin/dsm.

The <target> is specifier for the install directory on the remote dsm.  If
it does not have a colon, then a default install prefix of /opt/nidas is
appended.  If there is no <user>@ in the target, then root@ is added by
default.

This works best with public key authentication to the remote account:

    See: https://wiki.ucar.edu/display/SEW/Deploying+Files+to+a+DSM
    and: https://wiki.ucar.edu/pages/viewpage.action?pageId=376650544

Examples:

dsm_rsync.sh /tmp/cnidas/install/pi3 callab-dsm:/opt/nidas-dev

EOF
    exit 0
}

arch=""
source=""
target=""

# -a without group and owner, plus (D)evices, (H)ard links, (z)compression,
# and (h)uman-readable numbers.
opts="-zhvrlptDH"

while true; do
    case "$1" in 
        -h )
            showHelp
            ;;
        -a )
            echo "-a arch = $2"
            arch=$2
            shift 2
	    ;;
        -I )
	    opts="$opts -I"
            shift 1
            ;;
	-n )
	    opts="$opts -n"
	    shift
	    ;;
	-P )
	    opts="$opts -P"
	    shift
	    ;;
	-- )
	    shift
	    break
	    ;;
    esac
done;

if [ $# -ne 2 ]; then
    echo "not enough args: $@"
    showHelp
fi

source="$1"
target="$2"

# file /opt/local/nidas/bin/data_stats 
# /opt/local/nidas/bin/data_stats: ELF 64-bit LSB executable, x86-64, version 1 (GNU/Linux), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=ce4f51f04f0c3dc53028669ad5300315cc9007af, for GNU/Linux 3.2.0, with debug_info, not stripped
# file /home/granger/code/nidas/install/pi3/bin/data_stats 
# /home/granger/code/nidas/install/pi3/bin/data_stats: ELF 32-bit LSB pie executable, ARM, EABI5 version 1 (GNU/Linux), dynamically linked, interpreter /lib/ld-linux-armhf.so.3, for GNU/Linux 3.2.0, BuildID[sha1]=43fa258887395a46d834bf65d2c29786ca178f5c, with debug_info, not stripped

if [ -z "$arch" ]; then
    dsm="$source/bin/dsm"
    if [ ! -f "$dsm" ]; then
	echo "$dsm does not exist, cannot detect arch."
	exit 1
    fi
    arch=$(file $dsm)
fi
case "$arch" in

    *linux-x86-64*)
	echo "Source arch is x86_64."
	archlib=""
	;;
    *armhf*)
	echo "Source arch is armhf."
	archlib="arm-linux-gnueabihf"
	;;
    *armel*)
	echo "Source arch is armel."
        archlib="arm-linux-gnueabi"
	;;
    *)
	echo "Cannot derive arch lib dir from: $arch"
	exit 1
	;;
esac

if echo "$target" | egrep -q -v '@'; then
    target="root@$target"
fi
if echo "$target" | egrep -q -v ':'; then
    target="${target}:/opt/nidas"
fi


echo "Now running rsync ops..."
set -x
# Need to rsync to top-level target first, in case it does not exist yet,
# before copying to the library subdirectory.
rsync $opts "$source/bin" "$source/firmware" "$source/include" "$source/share" "$target"
# First create the lib subdirectory with rsync, then copy the source lib
# into the archlib directory.
rsync $opts --exclude="**" "$source/lib/" "$target/lib"
rsync $opts "$source/lib/" "$target/lib/$archlib"
