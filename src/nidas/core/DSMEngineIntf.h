/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef NIDAS_CORE_ENGINEINTF_H
#define NIDAS_CORE_ENGINEINTF_H

#include <nidas/core/XmlRpcThread.h>

#include <iostream>

namespace nidas { namespace core {

using namespace XmlRpc;

/// starts the DSMEngine
class Start : public XmlRpcServerMethod
{
public:
  Start(XmlRpcServer* s) : XmlRpcServerMethod("Start", s) {}
  void execute(XmlRpcValue& params, XmlRpcValue& result);
  std::string help() { return std::string("help Start"); }
};

/// stops the DSMEngine
class Stop : public XmlRpcServerMethod
{
public:
  Stop(XmlRpcServer* s) : XmlRpcServerMethod("Stop", s) {}
  void execute(XmlRpcValue& params, XmlRpcValue& result);
  std::string help() { return std::string("help Stop"); }
};

/// restarts the DSMEngine
class Restart : public XmlRpcServerMethod
{
public:
  Restart(XmlRpcServer* s) : XmlRpcServerMethod("Restart", s) {}
  void execute(XmlRpcValue& params, XmlRpcValue& result);
  std::string help() { return std::string("help Restart"); }
};

/// quits the DSMEngine
class Quit : public XmlRpcServerMethod
{
public:
  Quit(XmlRpcServer* s) : XmlRpcServerMethod("Quit", s) {}
  void execute(XmlRpcValue& params, XmlRpcValue& result);
  std::string help() { return std::string("help Quit"); }
};

/**
 * A thread that provides XML-based Remote Procedure Calls
 * to web interfaces from the DSMEngine.
 */
class DSMEngineIntf : public XmlRpcThread
{
public:
  DSMEngineIntf() : XmlRpcThread("DSMEngineIntf")
  {
      setCancelDeferred(true);
      setCancelEnabled(true);
  }

  int run() throw(nidas::util::Exception);
};

}}	// namespace nidas namespace core

#endif
