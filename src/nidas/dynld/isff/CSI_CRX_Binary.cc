// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   $LastChangedDate$

   $LastChangedRevision$

   $LastChangedBy$

   $HeadURL$

*/

#include <nidas/dynld/isff/CSI_CRX_Binary.h>
#include <nidas/util/EndianConverter.h>

#include <nidas/core/Variable.h>

using namespace nidas::dynld::isff;
using namespace nidas::core;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,CSI_CRX_Binary)

static const n_u::EndianConverter* fromBig =
    n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_BIG_ENDIAN);


CSI_CRX_Binary::CSI_CRX_Binary():
    _numOut(0),
    _sampleId(0),
    _badCRCs(0)
{
}

CSI_CRX_Binary::~CSI_CRX_Binary()
{
}

void CSI_CRX_Binary::validate()
    throw(n_u::InvalidParameterException)
{

    list<SampleTag*>& tags= getSampleTags();

    if (tags.size() != 1)
        throw n_u::InvalidParameterException(getName() +
                " must have one sample");

    const SampleTag* stag = tags.front();
    _numOut = stag->getVariables().size();
    _sampleId = stag->getId();

}

unsigned short CSI_CRX_Binary::signature(const unsigned char* buf, const unsigned char* eob)
{
    union {
        unsigned char s[2];
        unsigned short val;
    } sig;

    sig.val = 0xaaaa;

    unsigned char t1,t2;

    for ( ; buf < eob; buf++) {
        t1 = sig.s[1];
        sig.s[1] = sig.s[0];

        t2 = (sig.s[0] << 1) + ((sig.s[0] & 0x80) >> 7);

        sig.s[0] = t2 + t1 + *buf;
    }

    return sig.val;
}

bool CSI_CRX_Binary::reportBadCRC()
{
    if (!(_badCRCs++ % 1000))
            WLOG(("%s (CSI_CRX_Binary): %d CRC signature errors so far",
                        getName().c_str(),_badCRCs));
    return false;
}


bool CSI_CRX_Binary::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{
    const unsigned char* buf0 = (const unsigned char*) samp->getConstVoidDataPtr();
    unsigned int len = samp->getDataByteLength();


#ifdef CHECK_SIGNATURE
    if (len < 4) return false;  // at least the 2-byte signature and one word of data
    const unsigned char* eptr = buf0 + len - 2;   // pointer to signature

    unsigned short sval = fromBig->uint16Value(eptr);

    unsigned short cval = signature((const unsigned char*)buf0,(const unsigned char*)eptr);

    cerr << "signature=" << hex << sval << ", calc'd=" << cval << dec << endl;

    if (cval != sval) return reportBadCRC();
#else
    const unsigned char* eptr = buf0 + len;
#endif

    // new sample
    SampleT<float>* psamp = getSample<float>(_numOut);

    psamp->setTimeTag(samp->getTimeTag());
    psamp->setId(_sampleId);

    float* dout = psamp->getDataPtr();
    float* dend = dout + _numOut;

    // hi-res values have a 3 bit exponent, providing a range of 0-7, but values
    // 6 and 7 aren't supp-ota occur.
    static const float p10[] = {1,.1,.01,.001,.0001,.00001, floatNAN, floatNAN};

    for (const unsigned char* bptr = buf0 ; bptr < eptr && dout < dend; ) {
        unsigned char c = *bptr;

        // See section C.2 of CR10X or CR23X Operator's manual.
        // The manual gives letter names 'A' thru 'H' to bits 7-0.

        // check if bits 4,3,2 (aka DEF) are all ones
        if ((c & 0x1c) == 0x1c) {

            if ((c & 0xfc) == 0xfc) {
                // start of output array. bits 9-0 of 16 bit word are the output id

#ifdef DEBUG
                // just for initial curiosity's sake, print out the id
                if (bptr + 2 > eptr) break;
                short val = fromBig->int16Value(bptr);
                int outputid = (val & 0x3ff);
                cerr << "CSI_CRX_Binary: output array id=" << outputid << endl;
#endif

                bptr += 2;
            }
            else if ((c & 0x3c) == 0x1c) {
                // first byte of a 4 byte value
                if (bptr + 4 > eptr) break;
                int val = fromBig->int32Value(bptr);

                // check third byte of a 4 byte value
                bptr += 2;
                c = *bptr;
                bptr += 2;
                if ((c & 0xfc) != 0x3c) {
                    NLOG(("%s unrecognized 3rd byte #%lu: %#x",
                                getName().c_str(),(unsigned long)(bptr-buf0),
                                (unsigned int)c));
                    continue;
                }

                // this is a mess of a floating point format...
                int neg = (val & 0x40000000l);      /* sign bit */
                int exp = ((val & 0x03000000l) >> 23) +
                     ((val & 0x80000000l) >> 31);     /* exponent bits */
                val =  (val & 0x000000ffl) +
                    ((val & 0x00ff0000l) >> 8) +
                    ((val & 0x00000100l) << 8); /* mantissa */
                if (neg) val = -val;
                if (val == -99999 && exp == 0) *dout++ = floatNAN;
                else *dout++ = (float)val * p10[exp];
            }
            else if (c == 0x7f) {
                // first byte of dummy word
                bptr += 2;
                continue;
            }
            else {
                NLOG(("%s unrecognized byte #%lu: %#x",
                            getName().c_str(),(unsigned long)(bptr-buf0),
                            (unsigned int)c));
                bptr++;
            }
        }
        else {
            // bits 4,3,2 (aka DEF) not all ones, a two byte low resolution value.

            if (bptr + 2 > eptr) break;
            short val = fromBig->int16Value(bptr);
            bptr += 2;

            int neg = val & 0x8000;
            int exp = (val & 0x6000) >> 13;
            val &= 0x1fff;
            if (neg) val = -val;
            if (val == -6999 && exp == 0) *dout++ = floatNAN;
            else *dout++ = (float)val * p10[exp];
        }
    }
    for ( ; dout < dend; ) *dout++ = floatNAN;

    results.push_back(psamp);
    return true;
}
