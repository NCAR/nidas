// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 5; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
/*
 * Watlow
 *
 */

#include "WatlowCLS208.h"

#include <nidas/core/Variable.h>
#include <nidas/core/Sample.h>

#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

const n_u::EndianConverter* Watlow::_fromBig = n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_BIG_ENDIAN);
const n_u::EndianConverter* Watlow::_fromLittle = n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_LITTLE_ENDIAN);

NIDAS_CREATOR_FUNCTION_NS(raf,Watlow)

namespace {
    // string-ify CRC in little-endian order
    inline string formatCRC(uint16_t crc)
    {
        ostringstream ost;
        ost << hex << setw(2) << setfill('0') << (crc & 0xff) << ' ' << setw(2) << (crc >> 8);
        return ost.str();
    }
}

Watlow::Watlow(): _numWarnings(0)
{

// #define CALC_PROMPT_CRCS
#ifdef CALC_PROMPT_CRCS
    // show CRCs for prompt strings

    unsigned char prompt1[] = "\x01\x03\x01\x6B\x00\x08";
    cerr << "CRC=" << formatCRC(CRC16(prompt1,6)) << endl;
    cerr << "CRC_faster=" << formatCRC(CRC16_faster(prompt1,6)) << endl;

    unsigned char prompt2[] = "\x01\x03\x01\x4a\x00\x01";
    cerr << "CRC=" << formatCRC(CRC16(prompt2,6)) << endl;
    cerr << "CRC_faster=" << formatCRC(CRC16_faster(prompt2,6)) << endl;

    unsigned char prompt3[] = "\x01\x03\x01\xce\x00\x04";
    cerr << "CRC=" << formatCRC(CRC16(prompt3,6)) << endl;
    cerr << "CRC_faster=" << formatCRC(CRC16_faster(prompt3,6)) << endl;
#endif
}

uint16_t Watlow::CRC16(const unsigned char * input, int nbytes) throw()
{
    //"CRC is started by first preloading a 16 bit register to all 1's"  Manual pg 25
    uint16_t checksum = 0xffff;

    while (nbytes--)
    {
        checksum ^= *input++;
        for (int j = 0; j < 8; j++)
        {
	    uint16_t bit0 = checksum & 0x0001;
            checksum >>= 1; //shift bit right, add in 0 at left
            if (bit0)
            {
		//xor constant from http://docplayer.net/40721506-Cls200-mls300-and-cas200-communications-specification.html
                checksum ^= 0xa001;
            }
        }
    }

    return checksum;
}

uint16_t Watlow::CRC16_faster(const unsigned char *input, int nbytes) throw()
{
    /*
     * Faster version of CRC computation from
     * "Modicon Modbus Protocol Reference Guide", appendix C.
     */

    /* Table of CRC values for high–order byte */
    static unsigned char auchCRCHi[] = {
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
    } ;

    /* Table of CRC values for low–order byte */
    static unsigned char auchCRCLo[] = {
        0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04,
        0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8,
        0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC,
        0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3, 0x11, 0xD1, 0xD0, 0x10,
        0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
        0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38,
        0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C,
        0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26, 0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0,
        0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4,
        0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
        0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C,
        0xB4, 0x74, 0x75, 0xB5, 0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0,
        0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54,
        0x9C, 0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98,
        0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
        0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80, 0x40
    } ;

    unsigned char uchCRCHi = 0xFF;  /* high byte of CRC */
    unsigned char uchCRCLo = 0xFF;  /* low byte of CRC */
    unsigned char uIndex;           /* index into CRC lookup table */
    while (nbytes--) /* pass through message buffer */
    {
        uIndex = uchCRCHi ^ *input++ ;   /* calculate the CRC */
        uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex];
        uchCRCLo = auchCRCLo[uIndex];
    }

    // The Watlow stores the CRC in little endian order,
    // which I think is reverse that of the Modbus RTU standard.
    // (uchCRCHi << 8 | uchCRCLo) creates a CRC to match one stored
    // in big-endian order.
    // Flip the bytes in this calculation to match that returned by CRC16.
    return (uchCRCLo << 8 | uchCRCHi) ;
}

bool Watlow::checkCRC(const unsigned char * input, int nbytes, int sampnum) throw()
{

    uint16_t calcCRC = CRC16_faster(input, nbytes);

    uint16_t msgCRC = _fromLittle->uint16Value(input + nbytes);

    bool ok = calcCRC == msgCRC;

    if (!ok && !(_numWarnings++ % 1000))
        WLOG(("%s: Watlow warning #%u, bad CRC for sample %d, calculated=%s, received=%s",
            getName().c_str(), _numWarnings, sampnum,
            formatCRC(calcCRC).c_str(), formatCRC(msgCRC).c_str()));

    return ok;
}

bool Watlow::process(const Sample* samp,list<const Sample*>& results) throw()
{

    const unsigned char* som =
        (const unsigned char*) samp->getConstVoidDataPtr();

    // pointer to one past the last character in the raw sample
    const unsigned char* eod = som + samp->getDataByteLength();

    /* The Watlow is sampled with 3 prompts as defined in the XML header.
     * However NIDAS reads the result back as one raw sample, with all
     * three messages munged together.  Each returned message consists
     * of the address field (0x01), the function field
     * (0x03 is read holding register), the length in bytes, data
     * and a 2-byte CRC.
     *
     * message #0
     *	 prompt: holding register 0x016b, "Process Variable"
     *	    8 registers, signed 16 bit ints
     *	 message: 0x010310, 8*2=16 bytes of data, CRC. 21 bytes total
     * message #1
     *   prompt: holding register 0x014a, "Set Point"
     *      1 register, signed 16 bit int
     *   message: 0x010302, 1*2=2 bytes of data, CRC. 7 bytes total
     * message #2
     *	 prompt: holding register 0x01ce, "Output Value"
     *	    4 registers, unsigned 16 bit ints
     *   message: 0x010308, 4*2=8 bytes of data, CRC. 13 bytes total
     *
     * The NIDAS XML is configured to read back the data as one
     * 21+7+13=41 byte sample, * starting with 0x010310, followed by 38 bytes:
     *      <message separator='\x01\x03\x10" length="38" position="beg"/>
     */

    list<SampleTag*>& stags = getSampleTags();
    list<SampleTag*>::const_iterator si = stags.begin();

    unsigned int len = 0;

    for ( ; si != stags.end(); ++si, som += len + 5) {

	if (som + 3 > eod) break;   // first check leading three bytes

	const unsigned char *inp = som;

        // address field
	if (*inp++ != 0x01) break;

        // function field, 0x03=read holding register, 0x83 is exception
        unsigned char func = *inp++;

        // length field
        len = *inp++;

        SampleTag* stag = *si;
        int sampnum = stag->getId() - getId();

        // check function field
	if (func != 0x03) {
            // 0x83 is an exception response
            if (func == 0x83) {
                if (!(_numWarnings++ % 1000))
                    WLOG(("%s: Watlow warning #%u, function field=%#hhx (exception) in sample %d",
                        getName().c_str(), _numWarnings, func, sampnum));
                continue;
            }
            if (!(_numWarnings++ % 1000))
                WLOG(("%s: Watlow warning #%u, bad function field=%#hhx in sample %d",
                    getName().c_str(), _numWarnings, func, sampnum));
            break;
        }

        // check if there is enough input data for len data bytes + CRC
	if (inp + len + sizeof(int16_t) > eod) {
            if (!(_numWarnings++ % 1000))
                WLOG(("%s: Watlow warning #%u, truncated sample %d is %d bytes, should be %u, len=%u",
                    getName().c_str(), _numWarnings, sampnum, (int)(eod-som),
                    3 + len + sizeof(int16_t), len));
            break;
        }

	if(checkCRC(som, len + 3, sampnum)) {

            const std::vector<Variable*>& vars = stag->getVariables();
            unsigned int nvars = vars.size();

	    SampleT<float> * outs = getSample<float>(nvars);
	    outs->setTimeTag(samp->getTimeTag());
	    outs->setId(stag->getId());
	    float *dout = outs->getDataPtr();

            unsigned int iv;

            switch (sampnum) {
            case 1:
            case 2:
                for (iv = 0; iv < std::min(len / (unsigned int)sizeof(int16_t), nvars); iv++) {
                    float val = (float)_fromBig->int16Value(inp) / 10.0;
                    Variable* var = vars[iv];
                    VariableConverter* conv = var->getConverter();
                    if (conv) {
                        val = conv->convert(outs->getTimeTag(), val);
                    }
                    *dout++ = val;
                    inp += sizeof(int16_t);
                }
                break;
            case 3:
            default:
                for (iv = 0; iv < std::min(len / (unsigned int)sizeof(int16_t), nvars); iv++) {
                    float val = (float)_fromBig->uint16Value(inp);
                    Variable* var = vars[iv];
                    VariableConverter* conv = var->getConverter();
                    if (conv) {
                        val = conv->convert(outs->getTimeTag(), val);
                    }
                    *dout++ = val;
                    inp += sizeof(int16_t);
                }
                break;
            }
            // If more variables are configured than are found in the
            // input sample, set the extras to NAN
            for ( ; iv < nvars; iv++) {
                *dout++ = floatNAN;
            }
	    results.push_back(outs);
        }
    }

    return !results.empty();
}
