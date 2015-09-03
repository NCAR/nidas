// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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


#ifndef NIDAS_DYNLD_RAWSAMPLEINPUTSTREAM_H
#define NIDAS_DYNLD_RAWSAMPLEINPUTSTREAM_H

#include <nidas/dynld/SampleInputStream.h>

namespace nidas {

namespace core {
class IOChannel;
}

namespace dynld {

class RawSampleInputStream: public SampleInputStream
{
public:

    /**
     * Default constructor.
     */
    RawSampleInputStream();

    /**
     * Constructor with a connected IOChannel.
     * @param iochannel The IOChannel that we use for data input.
     *   RawSampleInputStream will own the pointer to the IOChannel,
     *   and will delete it in ~RawSampleInputStream().
     */
    RawSampleInputStream(nidas::core::IOChannel* iochannel);

    /**
     * Create a copy with a different, connected IOChannel.
     */
    RawSampleInputStream* clone(nidas::core::IOChannel*);

    virtual ~RawSampleInputStream();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:

    /**
     * Create a copy, but with a new IOChannel.
     */
    RawSampleInputStream(RawSampleInputStream&x,nidas::core::IOChannel*);

};

}}	// namespace nidas namespace core

#endif
