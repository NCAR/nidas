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

#include <nidas/dynld/isff/GOESXmtr.h>

#include <nidas/util/Logger.h>

// #include <iostream>
// #include <sstream>
// #include <iomanip>

using namespace std;
using namespace nidas::core;
using namespace nidas::dynld::isff;

namespace n_u = nidas::util;

// NIDAS_CREATOR_FUNCTION(GOESXmtr)

GOESXmtr::GOESXmtr():
    IOChannel(),_port(),_id(0),_channel(0),
    _xmitInterval(300),_xmitOffset(0),_statusFile()
{
}

GOESXmtr::GOESXmtr(const GOESXmtr& x):
    IOChannel(x),
    _port(x._port.getName()),
    _id(x._id),_channel(x._channel),
    _xmitInterval(x._xmitInterval),
    _xmitOffset(x._xmitOffset),
    _statusFile(x._statusFile)
{
}

GOESXmtr::~GOESXmtr()
{
}

void GOESXmtr::requestConnection(IOChannelRequester* rqstr)
    throw(nidas::util::IOException)
{
    open();
    rqstr->connected(this);
}

IOChannel* GOESXmtr::connect() throw(nidas::util::IOException)
{
    open();
    return this;
}


void GOESXmtr::open() throw(n_u::IOException)
{
    _port.open(O_RDWR | O_NOCTTY);
    _port.flushBoth();
}

void GOESXmtr::fromDOMElement(
	const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    XDOMElement xnode(node);

    n_u::Termios& tio = _port.termios();

    if(node->hasAttributes()) {
    // get all the attributes of the node
	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    const std::string& aname = attr.getName();
	    const std::string& aval = attr.getValue();

	    if (aname == "class");
	    else if (aname == "devicename") setName(aval);
	    else if (aname == "id") {
		istringstream ist(aval);
		unsigned long val;
		ist >> hex >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	string("GOESXmtr:") + getName(),
			aname,aval);
		setId(val);
	    }
	    else if (aname == "channel") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	string("GOESXmtr:") + getName(),
			aname,aval);
		setChannel(val);
	    }
	    else if (aname == "baud") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail() || tio.setBaudRate(val))
		    throw n_u::InvalidParameterException(
		    	string("GOESXmtr:") + getName(),
			aname,aval);
	    }
	    else if (aname == "parity") {
		if (aval == "odd") tio.setParity(n_u::Termios::ODD);
		else if (aval == "even") tio.setParity(n_u::Termios::EVEN);
		else if (aval == "none") tio.setParity(n_u::Termios::NONE);
		else throw n_u::InvalidParameterException(
		    string("GOESXmtr:") + getName(),
		    aname,aval);
	    }
	    else if (aname == "databits") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
			string("GOESXmtr:") + getName(),
		    	aname, aval);
		tio.setDataBits(val);
	    }
	    else if (aname == "stopbits") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
			string("GOESXmtr:") + getName(),
		    	aname, aval);
		tio.setStopBits(val);
	    }
	    else if (aname == "xmitInterval") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	string("GOESXmtr:") + getName(),
			aname,aval);
		setXmitInterval(val);
	    }
	    else if (aname == "xmitOffset") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	string("GOESXmtr:") + getName(),
			aname,aval);
		setXmitOffset(val);
	    }
	    else if (aname == "rfbaud") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	string("GOESXmtr:") + getName(),
			aname,aval);
		setRFBaud(val);
	    }
	    else if (aname == "statusFile") setStatusFile(aval);
	    else throw n_u::InvalidParameterException(
		string("GOESXmtr:") + getName(),
		"unknown attribute",aname);
	}
    }
}

