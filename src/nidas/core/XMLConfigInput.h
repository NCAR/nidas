/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef NIDAS_CORE_XMLCONFIGINPUT_H
#define NIDAS_CORE_XMLCONFIGINPUT_H

#include <nidas/util/McSocket.h>
#include <nidas/core/Datagrams.h>

namespace nidas { namespace core {

class XMLConfigInput: public nidas::util::McSocket
{
public:
    XMLConfigInput()
    {
	try {
            setInet4McastSocketAddress(
                nidas::util::Inet4SocketAddress(
                    nidas::util::Inet4Address::getByName(DSM_MULTICAST_ADDR),
                    DSM_SVC_REQUEST_PORT));
        }
        catch(const nidas::util::UnknownHostException& e) {
        }

        setRequestNumber(XML_CONFIG);
    }

protected:
};

}}	// namespace nidas namespace core

#endif
