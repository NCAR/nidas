/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_MCSOCKETREQUESTEROUTPUT_H
#define DSM_MCSOCKETREQUESTEROUTPUT_H

#include <DSMService.h>
#include <Output.h>
#include <DOMable.h>
#include <atdUtil/McSocket.h>

#include <string>
#include <iostream>

namespace dsm {

/**
 * Implementation of an Output, using McSocketRequester to request a connection
 */
class McSocketRequesterOutput: public Output, public atdUtil::McSocketRequester {

public:
    McSocketRequesterOutput(): socket(0) {}

    ~McSocketRequesterOutput() { delete socket; }

    const std::string& getName() const { return name; }

    void requestConnection(atdUtil::SocketAccepter *service,int pseudoPort)
    	throw(atdUtil::IOException);

    void offer(atdUtil::Socket* sock) throw(atdUtil::IOException);

    Output* clone() const;

    size_t getBufferSize() const;

    /**
    * Do the actual hardware write.
    */
    size_t write(const void* buf, size_t len) throw (atdUtil::IOException)
    {
      // std::cerr << "McSocketRequesterOutput::read, len=" << len << std::endl;
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
};

}

#endif
