/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifndef DSM_DSMSERVER_H
#define DSM_DSMSERVER_H

#ifdef NEEDED
#include <XMLFdFormatTarget.h>
#include <Datagrams.h>
#include <XMLStringConverter.h>

#include <xercesc/dom/DOMDocument.hpp>

#include <iostream>
#endif

#include <atdUtil/Thread.h>
#include <atdUtil/Socket.h>
#include <XMLConfigParser.h>
#include <XMLConfigWriter.h>

namespace dsm {

class SendXMLConfig: public atdUtil::Thread 
{
public:
    SendXMLConfig(xercesc::DOMNode* n,atdUtil::Socket s)
    	throw(atdUtil::IOException);
    int run() throw(atdUtil::Exception);

private:

    atdUtil::Socket sock;
    XMLConfigWriter writer;
    xercesc::DOMNode* node;
    atdUtil::Inet4SocketAddress addr;
};

class DSMConfigServer: public atdUtil::Thread {
public:

    DSMConfigServer(const std::string& xmlName);

    int run() throw(atdUtil::Exception);

    static time_t getFileModTime(const std::string& name) throw(atdUtil::IOException);
private:
    std::string xmlFileName;

};

}

#endif
