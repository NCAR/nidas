/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef NIDAS_CORE_DSMSERVERINTF_H
#define NIDAS_CORE_DSMSERVERINTF_H

#include <iostream>
#include <nidas/core/XmlRpcThread.h>

namespace nidas { namespace core {

using namespace XmlRpc;

/**
 * Measure a calm wind state for the LAMS sensor as a baseline.  This should be run
 * while the aircraft is NOT in motion.
 */
class MeasureBaselineLAMS : public XmlRpcServerMethod
{
public:
  MeasureBaselineLAMS(XmlRpcServer* s) : XmlRpcServerMethod("MeasureBaselineLAMS", s) {}
  void execute(XmlRpcValue& params, XmlRpcValue& result);
  std::string help() { return std::string("help MeasureBaselineLAMS"); }
};

/**
 * Remove the previously measured baseline for the LAMS sensor.  This should be run
 * while the aircraft is in flight.
 */
class SubtractBaselineLAMS : public XmlRpcServerMethod
{
public:
  SubtractBaselineLAMS(XmlRpcServer* s) : XmlRpcServerMethod("SubtractBaselineLAMS", s) {}
  void execute(XmlRpcValue& params, XmlRpcValue& result);
  std::string help() { return std::string("help SubtractBaselineLAMS"); }
};

/// gets a list of DSMs and their locations from the configuration
class GetDsmList : public XmlRpcServerMethod
{
public:
  GetDsmList(XmlRpcServer* s) : XmlRpcServerMethod("GetDsmList", s) {}
  void execute(XmlRpcValue& params, XmlRpcValue& result);
  std::string help() { return std::string("help GetDsmList"); }
};

/// list all of the NCAR A2D board's channels as a tree
class List_NCAR_A2Ds : public XmlRpcServerMethod
{
public:
  List_NCAR_A2Ds(XmlRpcServer* s) : XmlRpcServerMethod("List_NCAR_A2Ds", s) {}
  void execute(XmlRpcValue& params, XmlRpcValue& result);
  std::string help() { return std::string("help List_NCAR_A2Ds"); }
};

/**
 * A thread that provides XML-based Remote Procedure Calls
 * to web interfaces from the DSMServer.
 */
class DSMServerIntf : public XmlRpcThread
{
public:
  DSMServerIntf() : XmlRpcThread("DSMServerIntf") {}

  int run() throw(nidas::util::Exception);

};

}}	// namespace nidas namespace core

#endif
