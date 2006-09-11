#!/bin/bash
#
# /etc/init.d/emerald
#       Starts emerald driver module for querying/initializing/controlling
#	a Diamond Emerald 8 port serial card. Does a setserial on each uart
#	to notify the serial driver about the ports on the board.
#
# chkconfig: 2345 50 75
# description: Insert a driver module for an Emerald-MM-8 serial port card,
#		query available cards, do setserial on associated tty devices.
#

# set -x

RETVAL=0

PATH=/usr/local/bin:$PATH

# viper irqs associated with ISA irqs
# ISA  0 1 2 3   4    5   6   7  8  9   10  11  12    14  15
virqs=(0 0 0 104 105 106 107 108 0 112 109 110 111 0 113 114)
# IOPORTS on viper, under linux 2.6 are found starting at 0xf1000000
# 	under linux 2.4 are found starting at 0xf7000000
basepfx=0xf1000
#
# If CK jumper is installed in J9, then set baud_base=460800
# If CK jumper is not installed in J9, then set baud_base=115200
# baud_base=115200
baud_base=460800

# default configuration of first 4 boards
defports=(0x100 0x140 0x180 0x1c0)
defirqs=(3 7 5 4)

tmpdir=/var/tmp/dev

clear_tmpdir() {
    [ -d $tmpdir ] || mkdir -p $tmpdir
    rm -f $tmpdir/emerald*
    rm -f $tmpdir/ttyD*
}

get_nboards() {
    modprobe emerald
    emajor=`cat /proc/devices | fgrep emerald | cut -d\  -f1`

    # emerald major number may change from boot to boot
    # so we create these device files on $tmpdir
    # and create links to them from /dev.
    mknod $tmpdir/emerald0 c $emajor 0
    # number of boards with acceptable config in eeprom
    nboards=0
    nboards=`set_emerald -n $tmpdir/emerald0 || echo 0`

    # Create /dev/emeraldN for each board
    eminor=0
    for (( brd = 0; brd < $nboards; brd++ )); do
	dev=emerald$brd
	[ -c $tmpdir/$dev ] || mknod $tmpdir/$dev c $emajor $eminor
	[ -h /dev/$dev ] || ln -s $tmpdir/$dev /dev
	chmod 666 $tmpdir/$dev
	# increment eminor by number of uart ports
	while read portstr port irqstr irq; do
	    case "$irqstr" in
	    irq)
		let eminor++
		;;
	    esac
	done < <(set_emerald /dev/$dev)
    done
}

check_ports() {
    result=0
    ports=()
    for (( brd = 0; brd < $nboards; brd++ )); do
	dev=emerald$brd
        
	# read ioport and irq configuration
	while read portstr port irqstr irq; do
	    case "$irqstr" in
	    irq)
		# If a duplicate ioport address is found, report the error.
		if echo ${ports[*]} | fgrep -q $port; then
		    echo "Duplicate port=$port on /dev/$dev"
		    result=1
		    break
		fi
		ports=(${ports[*]} $port)
		# echo ${ports[*]}
		;;
	    esac
	done < <(set_emerald /dev/$dev)
    done
    return $result
}

create_ports() {

    # Create /dev/ttySN files and do setserial on them
    # First available ttyS port on Viper
    ttynum=5
    # major and minor number of ttyS5
    tmajor=4
    tminor=69
    # minor number of emerald devices
    eminor=0
    for (( brd = 0; brd < $nboards; brd++ )); do
	dev=emerald$brd

	# read ioport and irq configuration
	while read portstr port irqstr irq; do
	    case "$irqstr" in
	    irq)
		tty=/dev/ttyS$ttynum
		[ -c $tty ] || mknod $tty c $tmajor $tminor

		# device for accessing the digital out pin
		# associated with a port on the emerald
		dio=ttyD$ttynum
		[ -c $tmpdir/$dio ] || mknod $tmpdir/$dio c $emajor $eminor
		[ -h /dev/$dio ] || ln -s $tmpdir/$dio /dev

		nport=`echo $port | sed s/0x/$basepfx/`
		nirq=${virqs[$irq]}
		setserial -zvb $tty port $nport irq $nirq baud_base $baud_base autoconfig
		let eminor++
		let tminor++
		let ttynum++
		;;
	    esac
	done < <(set_emerald /dev/$dev)
    done
}

# Set EEPROM configuration on Emerald board to default values
default_eeprom_config() {
    for (( brd = 0; brd < $nboards; brd++ )); do
	dev=emerald$brd
	echo "doing: set_emerald $tmpdir/$dev ${defports[$brd]} ${defirqs[$brd]}"
	set_emerald $tmpdir/$dev ${defports[$brd]} ${defirqs[$brd]}
    done
}

case "$1" in
    start)
	clear_tmpdir
	get_nboards
	check_ports || default_eeprom_config
	create_ports
	;;
    stop)
	rmmod emerald
	;;
    default_eeprom)
	clear_tmpdir
	get_nboards
	default_eeprom_config
	;;
    *)
	echo "usage: $0 start|stop|default_eeprom"
	RETVAL=1
	;;
esac
exit $RETVAL

