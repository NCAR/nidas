/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
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

GOESXmtr::GOESXmtr():id(0),channel(0),
	xmitInterval(300),xmitOffset(0), rfBaud(100)
{
}

GOESXmtr::GOESXmtr(const GOESXmtr& x):
	port(x.port.getName()),
	id(x.id),channel(x.channel),
	xmitInterval(x.xmitInterval),xmitOffset(x.xmitOffset),
	rfBaud(x.rfBaud)
{
}

GOESXmtr::~GOESXmtr()
{
}

void GOESXmtr::requestConnection(ConnectionRequester* rqstr)
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
    port.open(O_RDWR | O_NOCTTY);

// #define DEBUG
#ifdef DEBUG
    cerr << "c_iflag=" << hex << port.iflag() << endl;
    cerr << "c_oflag=" << port.oflag() << endl;
    cerr << "c_cflag=" << port.cflag() << endl;
    cerr << "c_lflag=" << port.lflag() << endl;
    cerr << "cfgetispeed=" << dec << cfgetispeed(port.getTermios()) << endl;
    cerr << "baud rate=" << port.getBaudRate() << endl;
    cerr << "data bits=" << port.getDataBits() << endl;
    cerr << "stop bits=" << port.getStopBits() << endl;
    cerr << "parity=" << port.getParityString() << endl;

    cerr << "ICANON=" << (port.lflag() & ICANON) << endl;
#endif

    port.flushBoth();
}

void GOESXmtr::close() throw(n_u::IOException)
{
    port.close();
}

void GOESXmtr::fromDOMElement(
	const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    XDOMElement xnode(node);

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
		if (ist.fail() || port.setBaudRate(val))
		    throw n_u::InvalidParameterException(
		    	string("GOESXmtr:") + getName(),
			aname,aval);
	    }
	    else if (aname == "parity") {
		if (aval == "odd") port.setParity(port.ODD);
		else if (aval == "even") port.setParity(port.EVEN);
		else if (aval == "none") port.setParity(port.NONE);
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
		port.setDataBits(val);
	    }
	    else if (aname == "stopbits") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
			string("GOESXmtr:") + getName(),
		    	aname, aval);
		port.setStopBits(val);
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

