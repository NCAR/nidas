// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include <nidas/core/XmlRpcThread.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/Datagrams.h>
#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::core;
using namespace std;
using namespace XmlRpc;

XmlRpcThread::XmlRpcThread(const std::string& name):
    Thread(name), _xmlrpc_server(0)
{
    // unblock SIGUSR1 to register a signal handler, then block it
    // so that the pselect within XmlRpcDispatch will catch it.
    unblockSignal(SIGUSR1);
    blockSignal(SIGUSR1);
}

void XmlRpcThread::interrupt()
{
    // XmlRpcServer::exit() will not cause an exit of
    // XmlRpcServer::work(-1.0) if there are no rpc
    // requests coming in, but we'll do it anyway.
    if (_xmlrpc_server) _xmlrpc_server->exit();
    try {
        kill(SIGUSR1);
    }
    catch(const nidas::util::Exception& e) {
        WLOG(("XmlRpcThread interrupt: %s",e.what()));
    }
}

XmlRpcThread::~XmlRpcThread()
{
    // user must have done a join of this thread before calling
    // this destructor.
    if (_xmlrpc_server) _xmlrpc_server->shutdown();
    delete _xmlrpc_server;
}
