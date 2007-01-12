/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-07-24 07:51:51 -0600 (Mon, 24 Jul 2006) $

    $LastChangedRevision: 3446 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/SppSerial.cc $

*/

#include <nidas/dynld/raf/SppSerial.h>

#include <sstream>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,SppSerial)

const n_u::EndianConverter* SppSerial::toLittle = n_u::EndianConverter::getConverter(
    n_u::EndianConverter::EC_LITTLE_ENDIAN);


unsigned short SppSerial::computeCheckSum(unsigned char *pkt, int len)
{
    unsigned short local_len;
    unsigned short j, sum;

    // Get the length of the data bytes, minus the length of the checksum field.
    local_len = len - 2;

    // Sum the byte count and data bytes;
    for (j = 0, sum = 0; j < local_len; j++) {
        sum += (unsigned short)pkt[j];
    }
    return sum;
}
