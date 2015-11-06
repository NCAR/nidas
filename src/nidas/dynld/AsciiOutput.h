// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_ASCIIOUTPUT_H
#define NIDAS_DYNLD_ASCIIOUTPUT_H

#include <nidas/core/SampleOutput.h>

#include <iostream>

namespace nidas {

namespace core {
class SampleSource;
}

namespace dynld {

using namespace nidas::core;

class AsciiOutput: public SampleOutputBase
{
public:

    typedef enum format { DEFAULT, ASCII, HEX, SIGNED_SHORT, UNSIGNED_SHORT,
    	FLOAT, IRIG } format_t;

    AsciiOutput();

    AsciiOutput(IOChannel* iochannel,SampleConnectionRequester* rqstr=0);

    virtual ~AsciiOutput() {}

    /**
     * Implementation of SampleClient::flush().
     */
    void flush() throw() {}

    void requestConnection(SampleConnectionRequester* requester) throw();

    void connect(nidas::core::SampleSource* ) throw(nidas::util::IOException);
    /**
     * Set the format for character samples. Raw sensor samples
     * are character samples.
     */
    void setFormat(format_t val)
    {
        _format = val;
    }

    bool receive(const Sample* samp) throw();

protected:

    AsciiOutput* clone(IOChannel* iochannel);

    /**
     * Copy constructor, with a new IOChannel.
     */
    AsciiOutput(AsciiOutput&,IOChannel*);

    void printHeader() throw(nidas::util::IOException);

private:

    std::ostringstream _ostr;

    format_t _format;

    /**
     * Previous time tags by sample id. Used for displaying time diffs.
     */
    std::map<dsm_sample_id_t,dsm_time_t> _prevTT;

    bool _headerOut;

    /**
     * Copy constructor.
     */
    AsciiOutput(const AsciiOutput&);

};

}}	// namespace nidas namespace core

#endif
