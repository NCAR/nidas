#!/bin/bash

TEMP=`getopt --long -o "hfi:a:" "$@"`
eval set -- "$TEMP"

function showHelp()
{
    echo "Script to rsync the cross compiled ARM binaries to a DSM."
    echo "usage: ./dsm_rsync -i 192.168.1.160 -a [armhf|armel]<ret>"
    echo "       ./dsm_rsync --h<ret>"
    echo
    echo "This will not work, if you haven't shared a public key to the DSM's root user"
    echo "    See: https://wiki.ucar.edu/display/SEW/Deploying+Files+to+a+DSM"
    echo "    and: https://wiki.ucar.edu/pages/viewpage.action?pageId=376650544"
    exit 0
}

force=0

while true ; do
    case "$1" in 
        -i )
            #echo "IP = $2"
            ip=$2
            shift 2
        ;;
        -h )
            showHelp
        ;;
        -a )
            #echo "arch = $2"
            arch=$2
            shift 2
        ;;
        -f )
            force=1
            shift 1
        ;;
        * )
            break
        ;;
    esac
done;

if [[ $ip == "" ]] ; then
#    echo "IP: $ip"
    showHelp
fi

case "$arch" in 
    armhf )
        archlib="arm-linux-gnueabihf"
    ;;
    armel )
        archlib="arm-linux-gnueabi"
    ;;
    * )
        showHelp
    ;;
esac

opts="-azrvhe"
if [[ $force == 1 ]] ; then
 opts="$opts + I"
 echo $opts
fi

#echo "IP = $ip"
#echo "arch = $archlib"

rsync $opts ssh --progress /opt/nidas/bin  root@$ip:/opt/nidas
# put the libs in the arch-specific directory. In this case, it's the RPi w/the hardware float coproc.
ssh root@$ip mkdir -p /opt/nidas/lib/$archlib 
rsync $opts ssh --progress /opt/nidas/lib/*  root@$ip:/opt/nidas/lib/$archlib
rsync $opts ssh --progress /opt/nidas/firmware  root@$ip:/opt/nidas
rsync $opts ssh --progress /opt/nidas/include  root@$ip:/opt/nidas
rsync $opts ssh --progress /opt/nidas/share  root@$ip:/opt/nidas

