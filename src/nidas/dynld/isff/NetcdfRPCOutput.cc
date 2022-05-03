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


#include "NetcdfRPCOutput.h"

#ifdef HAVE_LIBNC_SERVER_RPC

#include "NetcdfRPCChannel.h"
#include <nidas/util/Logger.h>

using namespace nidas::dynld::isff;
using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,NetcdfRPCOutput)

NetcdfRPCOutput::NetcdfRPCOutput():
    SampleOutputBase(),_ncChannel(0),
    _startTime((time_t)0),_endTime((time_t)0)
{
}

NetcdfRPCOutput::NetcdfRPCOutput(IOChannel* ioc,SampleConnectionRequester* rqstr):
    SampleOutputBase(ioc,rqstr),
    _ncChannel(dynamic_cast<NetcdfRPCChannel*>(getIOChannel())),
    _startTime((time_t)0),_endTime((time_t)0)
{
    // setName(string("NetcdfRPCOutput: ") + getIOChannel()->getName());
    setName("NetcdfRPCOutput");
}

/* copy constructor */
NetcdfRPCOutput::NetcdfRPCOutput(NetcdfRPCOutput& x,IOChannel*ioc):
    SampleOutputBase(x,ioc),
    _ncChannel(dynamic_cast<NetcdfRPCChannel*>(getIOChannel())),
    _startTime((time_t)0),_endTime((time_t)0)
{
    // setName(string("NetcdfRPCOutput: ") + getIOChannel()->getName());
    setName("NetcdfRPCOutput");
}

NetcdfRPCOutput::~NetcdfRPCOutput()
{
}

void NetcdfRPCOutput::requestConnection(SampleConnectionRequester* requester)
{
    // NetcdfRPCChannel needs to know the SampleTags before it connects.
    list<const SampleTag*> tags = getSourceSampleTags();
    for (list<const SampleTag*>::const_iterator ti = tags.begin();
        ti != tags.end(); ++ti) _ncChannel->addSampleTag(*ti);
    SampleOutputBase::requestConnection(requester);
}

SampleOutput* NetcdfRPCOutput::connected(IOChannel* ioc)
{
    SampleOutput* so = SampleOutputBase::connected(ioc);
    if (so == this && !_ncChannel) setIOChannel(ioc);
    return so;
}

void NetcdfRPCOutput::setIOChannel(IOChannel* val)
{
    SampleOutputBase::setIOChannel(val);
    // setName(string("NetcdfRPCOutput: ") + getIOChannel()->getName());
    setName("NetcdfRPCOutput");
    _ncChannel = dynamic_cast<NetcdfRPCChannel*>(getIOChannel());
}

bool NetcdfRPCOutput::receive(const Sample* samp)
{
    dsm_time_t tt = samp->getTimeTag();
    if (_startTime && tt < _startTime)
        return true;
    if (_endTime && tt >= _endTime)
        return true;
    // cerr << "NetcdfRPCOutput::receive, samp=" << samp->getDSMId() << ',' << samp->getSpSId() << endl;
    try {
	_ncChannel->write(samp);
    }
    catch (const n_u::IOException& e) {
        PLOG(("%s", e.what()));
        disconnect();
        return false;
    }
    return true;
}

void NetcdfRPCOutput::fromDOMElement(const xercesc::DOMElement* node)
{
    // process <ncserver> tag
    int niochan = 0;
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();

        if (elname == "ncserver") {
	    IOChannel* ioc = new NetcdfRPCChannel();
	    ioc->fromDOMElement((xercesc::DOMElement*)child);
	    setIOChannel(ioc);
	}
	else throw n_u::InvalidParameterException(
                    "NetcdfRPCOutput::fromDOMElement",
		    "parse", "only supports ncserver elements");

        if (++niochan > 1)
            throw n_u::InvalidParameterException(
                    "NetcdfRPCOutput::fromDOMElement",
                    "parse", "must have only one child element");
    }
    if (!getIOChannel())
        throw n_u::InvalidParameterException(
                "NetcdfRPCOutput::fromDOMElement",
                "parse", "must have one child element");
    // setName(string("NetcdfRPCOutput: ") + getIOChannel()->getName());
    setName("NetcdfRPCOutput");
}


void
NetcdfRPCOutput::
setTimeClippingWindow(const nidas::util::UTime& startTime,
                      const nidas::util::UTime& endTime)
{
    _startTime = startTime.toUsecs();
    _endTime = endTime.toUsecs();
}

#endif  // HAVE_LIBNC_SERVER_RPC
