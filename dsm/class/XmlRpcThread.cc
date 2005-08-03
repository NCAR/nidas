/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <XmlRpcThread.h>
#include <iostream>
#include <DSMEngine.h>
#include <Datagrams.h>

using namespace dsm;
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
