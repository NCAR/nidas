// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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

#include "PPT_Serial.h"

#include <nidas/util/Logger.h>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,PPT_Serial)

PPT_Serial::PPT_Serial(): SerialSensor(), _numPromptsBack(0), _numParityErr(0), _numBuffErr(0) 
{
}

PPT_Serial::~PPT_Serial()
{
}


void PPT_Serial::open(int flags) throw(n_u::IOException)
{
    SerialSensor::open(flags);
}


void PPT_Serial::close() throw(n_u::IOException)
{
    if (_numPromptsBack > 0)  {
        n_u::Logger::getInstance()->log(LOG_ERR,
	      "%s: Number of Prompts recieved from PPT: %d",getName().c_str(),
			                _numPromptsBack);

    }
    SerialSensor::close();
}


bool PPT_Serial::process(const Sample * samp,
                           list < const Sample * >&results) throw()
{
    unsigned int nc = samp->getDataByteLength();
    if (nc == 0) return false;

    SampleT<char>* nsamp = getSample<char>(nc);

/**  Check on Bad return values and notify:
     * - Sometimes we see the prompt returned to the process method.  IF so 
         just track it rather than letting it pass along.
  */
    if (strncmp((const char*) samp->getConstVoidDataPtr(), PROMPT_PREFIX, strlen(PROMPT_PREFIX)) == 0) {
        if ((_numPromptsBack++ % 100) == 0 || _numPromptsBack < 10) {
            WLOG(("%s: Encountered prompt returned by the sensor - number: %d", 
                  getName().c_str(), _numPromptsBack));
            nsamp->freeReference();
	    return false;
        }
    }

    // Temperature over/under error or EEPROMP parity error
    if (strncmp((const char*) samp->getConstVoidDataPtr(), PARITY_ERROR, strlen(PARITY_ERROR)) == 0) {
        if ((_numParityErr++ % 100) == 0 || _numParityErr < 10) {
            WLOG(("%s: Encountered either Temperature over/under or EEPROMP parity error #: %d", 
                  getName().c_str(), _numParityErr));
            nsamp->freeReference();
            return false;
        }
    }

    // RS-232 Buffer space error
    if (strncmp((const char*) samp->getConstVoidDataPtr(), BUFFER_ERROR, strlen(BUFFER_ERROR)) == 0) {
        if ((_numBuffErr++ % 100) == 0 || _numBuffErr < 10) {
            WLOG(("%s: Encountered RS-232 Buffer Error #: %d", getName().c_str(), _numBuffErr));
            nsamp->freeReference();
            return false;
        }
    }

    nsamp->setTimeTag(samp->getTimeTag());
    nsamp->setId(samp->getId());

    // Fix incorrectly formated negative numbers
    // by skipping the spaces between the '-' and the digits.
    const char *cp = (const char*) samp->getConstVoidDataPtr();
    const char *ep = cp + nc;
    char *op = nsamp->getDataPtr();
    for ( ; cp < ep; ) {
        *op++ = *cp;
        if (*cp++ == '-') for ( ; cp < ep && *cp == ' '; cp++);
    }
    nsamp->setDataLength(op - (char*)nsamp->getDataPtr());

    bool rc = SerialSensor::process(nsamp, results);
    nsamp->freeReference();

    return rc;
}
