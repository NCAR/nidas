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
    if (l==8192) throw Exception(" char *buf exceeded!");

//    cerr << buf << endl;
    // convert char* buf into a parse-able memory stream
    MemBufInputSource* memBufIS = new MemBufInputSource
    (
       (const XMLByte*)buf
       , strlen(buf)
       , "fakeSysId"
       , false
    );
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

// ----------

void GetClocks::execute(XmlRpcValue& params, XmlRpcValue& result)
{
//cerr << "GetClocks" << endl;
  map<string, string>::iterator mi;
  for (mi  = _listener->_clocks.begin();
       mi != _listener->_clocks.end();    ++mi)
  {
    // only mark stalled numeric time changes as '-- stopped --'
    if (mi->second[0] == '2') // as in 2006 ...
      if (mi->second.compare(_listener->_oldclk[mi->first]) == 0) {
        if (_listener->_nstale[mi->first]++ > 3)
          mi->second = "----- stopped -----";
      }
      else
        _listener->_nstale[mi->first] = 0;

    _listener->_oldclk[mi->first] = mi->second;
    result[mi->first] = mi->second;
  }
}

void GetStatus::execute(XmlRpcValue& params, XmlRpcValue& result)
{
  std::string& arg = params[0];
//cerr << "GetStatus for " << arg << endl;
  result = _listener->_status[arg];
}
