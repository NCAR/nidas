/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/
#ifndef DSM_STATUSLISTENER_H
#define DSM_STATUSLISTENER_H

#include <xercesc/sax2/XMLReaderFactory.hpp> // provides SAX2XMLReader
#include <xmlrpc++/XmlRpc.h>

#include <atdUtil/Thread.h>

#include <iostream> // cerr
#include <map>

using namespace xercesc;
using namespace std;
using namespace XmlRpc;

namespace dsm {

class StatusHandler;
class GetClocks;
class GetStatus;


/// thread that listens to multicast messages from all of the DSMs.
class StatusListener: public atdUtil::Thread
{
  friend class StatusHandler;
  friend class GetClocks;
  friend class GetStatus;

public:
  StatusListener();
  ~StatusListener();

  int run() throw(atdUtil::Exception);

 private:
  /// this map contains the latest clock from each DSM
  map<string, string> _clocks;
  map<string, string> _oldclk;
  map<string, int>    _nstale;

  /// this map contains the latest status message from each DSM
  map<string, string> _status;

  /// SAX parser
  SAX2XMLReader* _parser;

  /// SAX handler
  StatusHandler* _handler;
};


/// gets a list of current clock times for each broadcasting DSM.
class GetClocks : public XmlRpcServerMethod
{
public:
  GetClocks(XmlRpcServer* s, StatusListener* lstn):
    XmlRpcServerMethod("GetClocks", s), _listener(lstn) {}

  void execute(XmlRpcValue& params, XmlRpcValue& result);

  std::string help() { return std::string("help GetClocks"); }

protected:
    /// reference to listener thread
    StatusListener* _listener;
};


/// gets a list of current status reports for each broadcasting DSM.
class GetStatus : public XmlRpcServerMethod
{
public:
  GetStatus(XmlRpcServer* s, StatusListener* lstn):
    XmlRpcServerMethod("GetStatus", s), _listener(lstn) {}

  void execute(XmlRpcValue& params, XmlRpcValue& result);

  std::string help() { return std::string("help GetStatus"); }

protected:
    /// reference to listener thread
    StatusListener* _listener;
};

}

#endif
