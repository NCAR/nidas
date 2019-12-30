#!/bin/bash

TEMP=`getopt --long -o "ufi:a:t:h:" "$@"`
eval set -- "$TEMP"

function showHelp()
{
    echo "Script to rsync the cross compiled ARM binaries to a DSM."
    echo "usage: ./dsm_rsync -i 192.168.1.160 -a [armhf|armel]<ret>"
    echo "usage: ./dsm_rsync -t tunnelname -h targetname -a [armhf|armel]<ret>"
    echo "       ./dsm_rsync --h<ret>"
    echo
    echo "-i: ip address for a local network."
    echo "-a: architecture of files to be xferred. Associates the correct destination directory."
    echo "-t: tunneled xfer. Causes tunnel to be built if it doesn't exist."
    echo "-h: remote host which is the tunnel target"
    echo "    Tunnel is then used to reach hostname on remote network"
    echo
    echo "This will not work, if you haven't shared a public key to the DSM's root user"
    echo "    See: https://wiki.ucar.edu/display/SEW/Deploying+Files+to+a+DSM"
    echo "    and: https://wiki.ucar.edu/pages/viewpage.action?pageId=376650544"
    echo
    echo "This will work a lot more efficiently, if the user has set up an ssh"
    echo "config file in ~/.ssh/config, which details various tunnels and target "
    echo "hosts."
    echo
    echo "ssh config example:"
    echo
    [[ -e ~/.ssh/config ]] && cat ~/.ssh/config
    exit 0
}

force=0
arch=""

echo "Number of args: $#"
echo "First arg: $1"
if [[ "$1" != "-?" && $# < 5 ]] ; then
    echo "not enough args: $@"
    showHelp
fi

echo "Parsing command line args..."
while true ; do
    case "$1" in 
        -i )
            echo "-i = $2"
            ip=$2
            shift 2
        ;;
        -u )
            echo "Request usage..."
            showHelp
        ;;
        -t )
            echo "-t tunnel host = $2"
            tunnel="$2"
            echo "\$tunnel: $tunnel"
            shift 2
        ;;
        -h )
            echo "-h target host = $2"
            host="$2"
            echo "\$host: $host"
            shift 2
        ;;
        -a )
            echo "-a arch = $2"
            arch=$2
            shift 2
        ;;
        -f )
            force=1
            shift 1
        ;;
        * )
            echo "Unknown arg: $1"
            break
        ;;
    esac
done;

echo "Done parsing args..."

tunnelPortArg=""
target=""
archLib=""

if [[ $tunnel != "" && $host != "" ]] ; then
    echo "Tunnel operation in progress..."
    tunnelPort=2222
    tunnelPortArg="-p $tunnelPort"
    isTunneled=`ps aux | grep -c "ssh ustar-tunnel -L $tunnelPort:$host:22 -fN"`
    #echo "isTunneled: ${isTunneled}"
    if [[ ${isTunneled} < 2 ]] ; then
        echo "Starting tunnel..."
        ssh $tunnel -L $tunnelPort:$host:22 -fN 
    else
        echo "Tunnel already running..."
    fi

    target="$host"
else
    echo "Not tunneling..."

    if [[ $ip = "" ]] ; then
        echo "When not tunneling -i must be provided as a numeric tcp/ip address or a host name"
        showHelp
    else
        target="$ip"
    fi
fi

if [[ $arch == "" ]] ; then
    echo "-a argument is required"
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
        echo "Unknown architecture: $arch"
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

echo "Now running rsync ops..."

rsync $opts "ssh $tunnelPortArg" --progress /opt/nidas/bin root@$target:/opt/nidas
# put the libs in the arch-specific directory. In this case, its the RPi w/the hardware float coproc.
ssh $tunnelPortArg root@$target mkdir -p /opt/nidas/lib/$archlib 
rsync $opts "ssh $tunnelPortArg" --progress /opt/nidas/lib/*     root@$target:/opt/nidas/lib/$archlib
rsync $opts "ssh $tunnelPortArg" --progress /opt/nidas/firmware  root@$target:/opt/nidas
rsync $opts "ssh $tunnelPortArg" --progress /opt/nidas/include   root@$target:/opt/nidas
rsync $opts "ssh $tunnelPortArg" --progress /opt/nidas/share     root@$target:/opt/nidas

