// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/SocketIODevice.h>
#include <nidas/util/BluetoothRFCommSocketAddress.h>

#include <nidas/util/Logger.h>

#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SocketIODevice::SocketIODevice():
    _addrtype(-1),_desthost(),_port(-1),_sockAddr(),_bindAddr()
{
}

SocketIODevice::~SocketIODevice()
{
}

/* static */
void SocketIODevice::parseAddress(const string& name, int& addrtype,
    string& desthost, int& port, string& bindaddr) throw(n_u::ParseException)
{
    string::size_type idx = name.find(':');
    addrtype = -1;
    desthost = string();
    port = -1;
    if (idx != string::npos) {
	string field = name.substr(0,idx);
	if (field == "inet" || field == "sock" || field == "usock")
            addrtype = AF_INET;
	else if (field == "unix") addrtype = AF_UNIX;
        // Docs about the Java API for bluetooth mention "btspp:" in the URL,
        // meaning Bluetooth Serial Port Profile. 
#ifdef HAVE_BLUETOOTH_RFCOMM_H
	else if (field == "btspp") addrtype = AF_BLUETOOTH;
#endif
	idx++;
    }
    if (addrtype < 0) {
#ifdef HAVE_BLUETOOTH_RFCOMM_H
        const char* msg = "address type prefix should be \"inet:\", \"sock:\", \"unix:\", \"usock:\" or \"btspp:\"";
#else
        const char* msg = "address type prefix should be \"inet:\", \"sock:\", \"unix:\" or \"usock:\"";
#endif
	throw n_u::ParseException(name,msg);
    }

    if (addrtype == AF_UNIX) desthost = name.substr(idx);
    else if (addrtype == AF_INET){
	string::size_type idx2 = name.find(':',idx);
	if (idx2 != string::npos) {
	    desthost = name.substr(idx,idx2-idx);
	    string portstr = name.substr(idx2+1);
	    istringstream ist(portstr);
	    ist >> port;
	    if (ist.fail()) port = -1;
	}
    }
#ifdef HAVE_BLUETOOTH_RFCOMM_H
    else if (addrtype == AF_BLUETOOTH){

        const char* exprs[] = {
            /* address of 6 hex bytes separated by colons, followed by optional  ":chan" and optional ":hciN" */
            "^btspp:(([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2})(:([0-9]*)(:hci([0-9]+))*)*$",
            /* friendly name not containing a colon, followed by optional  ":chan" and optional ":hciN" */
            "^btspp:([^:]+)(:([0-9]*)(:hci([0-9]+))*)*$"
        };

        for (unsigned int i = 0; i < sizeof(exprs) / sizeof(exprs[0]); i++) {
            int regstatus;
            regex_t addrPreg;
            regmatch_t pmatch[7];
            int nmatch = sizeof pmatch/ sizeof(regmatch_t);

            if ((regstatus = ::regcomp(&addrPreg,exprs[i],REG_EXTENDED)) != 0) {
                char regerrbuf[64];
                regerror(regstatus,&addrPreg,regerrbuf,sizeof regerrbuf);
                throw n_u::ParseException("Bluetooth address regular expression",
                    string(regerrbuf));
            }

            regstatus = ::regexec(&addrPreg,name.c_str(),nmatch, pmatch,0);
            regfree(&addrPreg);

            port = 1;
            
            if (regstatus == 0 && pmatch[0].rm_so >= 0 && pmatch[0].rm_eo > pmatch[0].rm_so) {
                desthost = name.substr(pmatch[1].rm_so,pmatch[1].rm_eo - pmatch[1].rm_so);
                int ci = 4; // index of channel number field
                int hi = 6; // index of hciN field
                if (i > 0) {
                    ci = 3;
                    hi = 5;
                }

                if (pmatch[ci].rm_so >= 0 && pmatch[ci].rm_eo > pmatch[ci].rm_so) {
                    string tmpstr = name.substr(pmatch[ci].rm_so,pmatch[ci].rm_eo - pmatch[ci].rm_so);
                    port = atoi(tmpstr.c_str());
                }

                if (pmatch[hi].rm_so >= 0 && pmatch[hi].rm_eo > pmatch[hi].rm_so)
                    bindaddr = name.substr(pmatch[hi].rm_so,pmatch[hi].rm_eo - pmatch[hi].rm_so);
                break;
            }
        }

    }
#endif
    // check for empty desthost, except in the case of inet sockets,
    // where desthost can be empty because the ServerSocket is listening
    // on INADDR_ANY.
    if (addrtype != AF_INET && desthost.length() == 0)
	throw n_u::ParseException(name,
	    string("cannot parse host/path in socket address: ") + name);
    if (addrtype == AF_INET && port < 0)
	throw n_u::ParseException(name,
            string("cannot parse port number in address: ") + name);
#ifdef HAVE_BLUETOOTH_RFCOMM_H
    if (addrtype == AF_BLUETOOTH && port < 0)
	throw n_u::ParseException(name,
            string("cannot parse channel number in address: ") + name);
#endif
}

void SocketIODevice::open(int /* flags */)
	throw(n_u::IOException,n_u::InvalidParameterException)
{
    if (_addrtype < 0) {
	try {
	    parseAddress(getName(),_addrtype,_desthost,_port,_bindAddr);
	}
	catch(const n_u::ParseException &e) {
	    throw n_u::InvalidParameterException(getName(),"name",e.what());
	}
    }
    if (_addrtype == AF_INET) {
	try {
	    _sockAddr.reset(new n_u::Inet4SocketAddress(
		n_u::Inet4Address::getByName(_desthost),_port));
	}
	catch(const n_u::UnknownHostException &e) {
	    throw n_u::InvalidParameterException(getName(),"name",e.what());
	}
    }
#ifdef HAVE_BLUETOOTH_RFCOMM_H
    else if (_addrtype == AF_BLUETOOTH) {
	try {
	    _sockAddr.reset(new n_u::BluetoothRFCommSocketAddress(
		n_u::BluetoothAddress::getByName(_desthost),_port));
	}
#ifdef THROW_INVALID_PARAM
	catch(const n_u::UnknownHostException &e) {
	    throw n_u::InvalidParameterException(getName(),"name",e.what());
	}
#else
	catch(const n_u::UnknownHostException &e) {
	    throw n_u::IOException(getName(),_desthost,EADDRNOTAVAIL);
	}
#endif
    }
#endif
    else if (_addrtype == AF_UNIX)
        _sockAddr.reset(new n_u::UnixSocketAddress(_desthost));
    else throw n_u::InvalidParameterException(getName(),"name","unsupported address type");
}

