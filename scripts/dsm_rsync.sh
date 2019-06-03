#!/bin/bash

TEMP=`getopt --long -o "hi:a:" "$@"`
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

while true ; do
    case "$1" in 
        -i )
            ip=$2
            shift 2
        ;;
        -h )
            showHelp
        ;;
        -a )
            arch=$2
            shift 2
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

rsync -azrvhe ssh --progress /opt/nidas/bin  root@$ip:/opt/nidas
# put the libs in the arch-specific directory. In this case, it's the RPi w/the hardware float coproc.
rsync -azrvhe ssh --progress /opt/nidas/lib/*  root@$ip:/opt/nidas/lib/$archlib
rsync -azrvhe ssh --progress /opt/nidas/firmware  root@$ip:/opt/nidas
rsync -azrvhe ssh --progress /opt/nidas/include  root@$ip:/opt/nidas
rsync -azrvhe ssh --progress /opt/nidas/share  root@$ip:/opt/nidas

