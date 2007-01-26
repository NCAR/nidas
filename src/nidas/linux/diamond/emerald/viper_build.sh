#!/bin/sh

PATH=/opt/arcom/bin:$PATH

# kdir=/home/maclean/viper/linux/linux-source-2.6.11.11-arcom2
kdir=/usr/local/arcom/ael/kernel/2.6.11/linux-source-2.6.11.11-arcom2_pps
kdir=/usr/local/arcom/ael/kernel/2.6.16/linux-source-2.6.16.28-arcom1_pps


# make ARCH=arm KERNELDIR=$kdir CROSS_COMPILE=arm-linux- emerald.o

# make ARCH=arm AS=arm-linux-gas CC=arm-linux-gcc LD=arm-linux-ld -C $kdir M=`pwd` modules
make ARCH=arm CROSS_COMPILE=arm-linux- KERNELDIR=$kdir

[ -d ../../../../build_arm/linux ] || mkdir ../../../../build_arm/linux

mv emerald.ko ../../../../build_arm/linux

