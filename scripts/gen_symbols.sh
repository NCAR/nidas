#!/bin/sh

# hack script to get the list of symbols in the nidas libraries

tmpfile=$(mktemp /tmp/${0##*/}_XXXXXX)

dpkg-gensymbols -pnidas-libs -Idebian/nidas-libs.symbols.armel -Pdebian/nidas-libs \
    -edebian/nidas-libs/opt/nidas/lib/arm-linux-gnueabi/libnidas.so.1.2\
    -edebian/nidas-libs/opt/nidas/lib/arm-linux-gnueabi/libnidas_dynld.so.1.2\
    -edebian/nidas-libs/opt/nidas/lib/arm-linux-gnueabi/libnidas_util.so.1.2\
    -edebian/nidas-libs/opt/nidas/lib/arm-linux-gnueabi/nidas_dynld_iss_TiltSensor.so.1.2\
    -edebian/nidas-libs/opt/nidas/lib/arm-linux-gnueabi/nidas_dynld_iss_WICORSensor.so.1.2 \
    > $tmpfile

patch debian/nidas-libs.symbols.armel $tmpfile



