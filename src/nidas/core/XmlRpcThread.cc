/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/XmlRpcThread.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/Datagrams.h>

#include <iostream>

using namespace nidas::core;
using namespace std;
using namespace XmlRpc;


XmlRpcThread::XmlRpcThread(const std::string& name):
  Thread(name), _xmlrpc_server(0)
{
  blockSignal(SIGINT);
  blockSignal(SIGHUP);
  blockSignal(SIGTERM);
}


XmlRpcThread::~XmlRpcThread()
{
  if (isRunning()) {
    cancel();
    join();
  }
  delete _xmlrpc_server;
}
