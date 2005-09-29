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

#include <IOChannel.h>
#include <DOMable.h>
#include <atdUtil/McSocket.h>

#include <string>
#include <iostream>

namespace dsm {

/**
 * Implementation of an IOChannel, using McSocket to establish connections
 */
class McSocket: public IOChannel, public atdUtil::McSocket {

public:

    /**
     * Constructor.
     */
    McSocket();

    /**
     * Copy constructor. Should only be called before atdUtil::Socket
     * is connected.
     */
    McSocket(const McSocket&);

    /**
     * Copy constructor, with a new connnected atdUtil::Socket
     */
    McSocket(const McSocket&,atdUtil::Socket*);

    ~McSocket() { delete socket; }

    McSocket* clone() const;

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

    IOChannel* connect(int pseudoPort) throw(atdUtil::IOException);

    virtual bool isNewFile() const { return newFile; }

    void connected(atdUtil::Socket* sock);

    atdUtil::Inet4Address getRemoteInet4Address() const throw();

    size_t getBufferSize() const throw();

    /**
     * Do the actual hardware read.
     */
    size_t read(void* buf, size_t len) throw (atdUtil::IOException);

    /**
     * Do the actual hardware write.
     */
    size_t write(const void* buf, size_t len) throw (atdUtil::IOException)
    {
	return socket->send(buf,len,MSG_DONTWAIT | MSG_NOSIGNAL);
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

    bool firstRead;

    bool newFile;
};

}

#endif
