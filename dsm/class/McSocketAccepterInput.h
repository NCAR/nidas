/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_MCSOCKETACCEPTERINPUT_H
#define DSM_MCSOCKETACCEPTERINPUT_H

#include <DSMService.h>
#include <Input.h>
#include <DOMable.h>
#include <atdUtil/McSocket.h>

#include <string>
#include <iostream>

namespace dsm {

/**
 * Implementation of an Input, using McSocketAccepter to listen for connections
 */
class McSocketAccepterInput: public Input, public atdUtil::McSocketAccepter {

public:
    McSocketAccepterInput(): socket(0) {}

    ~McSocketAccepterInput() { delete socket; }

    const std::string& getName() const { return name; }

    void requestConnection(atdUtil::SocketAccepter *service,int pseudoPort)
    	throw(atdUtil::IOException);

    int getInputPseudoPort() const { return getPseudoPort(); }

    Input* clone() const;

    void offer(atdUtil::Socket* sock) throw(atdUtil::IOException);

    size_t getBufferSize() const;

    /**
    * Do the actual hardware read.
    */
    size_t read(void* buf, size_t len) throw (atdUtil::IOException)
    {
	return socket->recv(buf,len);
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
};

}

#endif
