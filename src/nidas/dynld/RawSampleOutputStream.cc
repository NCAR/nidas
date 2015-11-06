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

#include <nidas/dynld/RawSampleOutputStream.h>
#include <nidas/core/Datagrams.h>

#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(RawSampleOutputStream)

RawSampleOutputStream::RawSampleOutputStream(): SampleOutputStream() 
{
}

RawSampleOutputStream::RawSampleOutputStream(IOChannel* i,SampleConnectionRequester* rqstr):
    SampleOutputStream(i,rqstr)
{
    setName("RawSampleOutputStream: " + getIOChannel()->getName());
}

RawSampleOutputStream::RawSampleOutputStream(RawSampleOutputStream& x,
	IOChannel* iochannel):
	SampleOutputStream(x,iochannel)
{
    setName("RawSampleOutputStream: " + getIOChannel()->getName());
}

RawSampleOutputStream::~RawSampleOutputStream()
{
}

RawSampleOutputStream* RawSampleOutputStream::clone(IOChannel* iochannel)
{
    return new RawSampleOutputStream(*this,iochannel);
}

void RawSampleOutputStream::fromDOMElement(const xercesc::DOMElement* node)
        throw(n_u::InvalidParameterException)
{
    SampleOutputStream::fromDOMElement(node);
    if (getIOChannel()->getRequestType() < 0)
    	getIOChannel()->setRequestType(RAW_SAMPLE);
}
