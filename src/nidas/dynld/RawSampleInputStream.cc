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

#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/core/IOChannel.h>

#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(RawSampleInputStream)

RawSampleInputStream::RawSampleInputStream(): SampleInputStream(true)
{
}

RawSampleInputStream::RawSampleInputStream(nidas::core::IOChannel* iochan):
	SampleInputStream(iochan,true)
{
}

RawSampleInputStream::RawSampleInputStream(RawSampleInputStream& x,
	nidas::core::IOChannel* iochannel):
	SampleInputStream(x,iochannel)
{
}

/**
 * Create a clone, with a new, connected IOChannel.
 */
RawSampleInputStream* RawSampleInputStream::clone(nidas::core::IOChannel* iochannel)
{
    return new RawSampleInputStream(*this,iochannel);
}


RawSampleInputStream::~RawSampleInputStream()
{
}

void RawSampleInputStream::fromDOMElement(const xercesc::DOMElement* node)
        throw(n_u::InvalidParameterException)
{
    SampleInputStream::fromDOMElement(node);
    if (_iochan->getRequestType() < 0)
    	_iochan->setRequestType(RAW_SAMPLE);
}
