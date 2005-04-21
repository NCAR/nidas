/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_MCSOCKET_H
#define DSM_MCSOCKET_H

#include <DSMService.h>
#include <IOChannel.h>
#include <DOMable.h>
#include <atdUtil/McSocket.h>

#include <string>
#include <iostream>

namespace dsm {

/**
 * Implementation of an IOChannel, using McSocket to listen for connections
 */
class McSocket: public IOChannel, public atdUtil::McSocket {

public:

    /**
     * Constructor.
     */
    McSocket(): socket(0),connectionRequester(0),amRequester(true) {}

    ~McSocket() { delete socket; }

    /**
     * Does this McSocket request connections, or does it
     * listen for incoming connections.
     */
    bool isRequester() const { return amRequester; }

    void setRequester(bool val) { amRequester = val; }

    void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

    void requestConnection(ConnectionRequester* service,int pseudoPort)
    	throw(atdUtil::IOException);

    IOChannel* clone();

    void connected(atdUtil::Socket* sock);

    atdUtil::Inet4Address getRemoteInet4Address() const {
        return socket->getInet4Address();
    }

    size_t getBufferSize() const throw();

    /**
    * Do the actual hardware read.
    */
    size_t read(void* buf, size_t len) throw (atdUtil::IOException)
    {
	size_t res = socket->recv(buf,len);
#ifdef DEBUG
	std::cerr << "McSocket::read, len=" << len << " res=" << res << std::endl;
#endif
	return res;
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const void* buf, size_t len) throw (atdUtil::IOException)
    {
#ifdef DEBUG
	std::cerr << "McSocket::write, len=" << len << std::endl;
#endif
	return socket->send(buf,len);
    }

    void close() throw (atdUtil::IOException);

    int getFd() const;

    void fromDOMElement(const xercesc::DOMElement*)
        throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
        toDOMParent(xercesc::DOMElement* parent)
                throw(xercesc::DOMException);

    xercesc::DOMElement*
        toDOMElement(xercesc::DOMElement* node)
                throw(xercesc::DOMException);

protected:
    atdUtil::Socket* socket;
    std::string name;

    ConnectionRequester* connectionRequester;

    bool amRequester;
};

}

#endif
