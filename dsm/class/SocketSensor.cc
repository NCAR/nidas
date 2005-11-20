/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <SocketSensor.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_FUNCTION(SocketSensor)

SocketSensor::SocketSensor():
    addrtype(-1),destport(-1)
{
}

SocketSensor::~SocketSensor()
{
    close();
}

void SocketSensor::parseAddress(const string& name)
	throw(atdUtil::ParseException)
{
    size_t idx = name.find(':');
    addrtype = -1;
    desthost = string();
    destport = -1;
    if (idx != string::npos) {
	string field = name.substr(0,idx);
	if (!field.compare("inet")) addrtype = AF_INET;
	else if (!field.compare("unix")) addrtype = AF_UNIX;
	idx++;
    }
    if (addrtype < 0)
	throw atdUtil::ParseException(name,
		"address type prefix should be \"inet:\" or \"unix:\"");

    if (addrtype == AF_UNIX) desthost = name.substr(idx);
    else {
	size_t idx2 = name.find(':',idx);
	if (idx2 != string::npos) {
	    desthost = name.substr(idx,idx2-idx);
	    string portstr = name.substr(idx2+1);
	    cerr << "portstr=" << portstr << endl;
	    istringstream ist(portstr);
	    ist >> destport;
	    if (ist.fail()) destport = -1;
	}
    }
    if (desthost.length() == 0)
	throw atdUtil::ParseException(name,
	    "cannot parse host name");
    if (addrtype == AF_INET && destport < 0)
	throw atdUtil::ParseException(name,"cannot parse port");

}

void SocketSensor::open(int flags)
	throw(atdUtil::IOException,atdUtil::InvalidParameterException)
{
    if (addrtype < 0) {
	try {
	    parseAddress(getDeviceName());
	}
	catch(const atdUtil::ParseException &e) {
	    throw atdUtil::InvalidParameterException(e.what());
	}
    }
    if (addrtype == AF_INET) {
	try {
	    sockAddr.reset(new atdUtil::Inet4SocketAddress(
		atdUtil::Inet4Address::getByName(desthost),destport));
	}
	catch(const atdUtil::UnknownHostException &e) {
	    throw atdUtil::InvalidParameterException(e.what());
	}
    }
    else sockAddr.reset(new atdUtil::UnixSocketAddress(desthost));

    socket.reset(new atdUtil::Socket());
    socket->connect(*sockAddr.get());
}

dsm_time_t SocketSensor::readSamples(SampleDater* dater)
           throw(atdUtil::IOException)
{
    if (MessageStreamSensor::getMessageSeparatorAtEOM())
	return readSamplesSepEOM(dater,this);
    else
	return readSamplesSepEOM(dater,this);
}

void SocketSensor::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    DSMSensor::fromDOMElement(node);
    MessageStreamSensor::fromDOMElement(node);
    try {
        parseAddress(getDeviceName());
    }
    catch(const atdUtil::ParseException& e) {
        throw atdUtil::InvalidParameterException(
		getDeviceName(),"devname",e.what());
    }
}

DOMElement* SocketSensor::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* SocketSensor::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

