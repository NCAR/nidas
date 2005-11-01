/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/
#include <xercesc/util/OutOfMemoryException.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>

#include <atdUtil/Socket.h>

#include <Datagrams.h>
#include <XMLStringConverter.h>
#include <StatusListener.h>
#include <StatusHandler.h>

#include <iostream>

using namespace atdUtil;
using namespace std;
using namespace dsm;

StatusListener::StatusListener():Thread("StatusListener")
{
  // initialize the XML4C2 system for the SAX2 parser
  try
  {
    XMLPlatformUtils::Initialize();
  }
  catch (const XMLException& toCatch)
  {
    cerr << "Error during initialization! :"
         << XMLStringConverter(toCatch.getMessage()) << endl;
    return;
  }
  // create a SAX2 parser object
  _parser = XMLReaderFactory::createXMLReader();
  _handler = new StatusHandler(this);
  _parser->setContentHandler(_handler);
  _parser->setLexicalHandler(_handler);
  _parser->setErrorHandler(_handler);
}

StatusListener::~StatusListener()
{
  XMLPlatformUtils::Terminate();
  delete _handler;
  delete _parser;
}

int StatusListener::run() throw(Exception)
{
  // create a socket to listen for the XML status messages
  MulticastSocket msock(DSM_MULTICAST_STATUS_PORT);
  msock.joinGroup(Inet4Address::getByName(DSM_MULTICAST_ADDR));
  Inet4SocketAddress from;
  char buf[8192];

  for (;;) {
    // blocking read on multicast socket
    size_t l = msock.recvfrom(buf,sizeof(buf),0,from);
    string IP = from.getInet4Address().getHostName();
    if (l==8192) throw Exception(" char *buf exceeded!");

//     cerr << "[" << IP << "] " << buf << endl;
    // convert char* buf into a parse-able memory stream
    MemBufInputSource* memBufIS = new MemBufInputSource
    (
       (const XMLByte*)buf
       , strlen(buf)
       , "fakeSysId"
       , false
    );
    _handler->setSource(IP);
    try {
      _parser->parse(*memBufIS);
    }
    catch (const OutOfMemoryException&) {
        cerr << "OutOfMemoryException" << endl;
        delete memBufIS;
        return 0;
    }
    catch (const XMLException& e) {
        cerr << "\nError during parsing memory stream:\n"
             << "Exception message is:  \n"
             << XMLStringConverter(e.getMessage()) << "\n" << endl;
        delete memBufIS;
        return 0;
    }
    delete memBufIS;
  }
}
