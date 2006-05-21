/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef DSM_XMLCONFIGINPUT_H
#define DSM_XMLCONFIGINPUT_H

#include <atdUtil/McSocket.h>
#include <Datagrams.h>

namespace dsm {

class XMLConfigInput: public atdUtil::McSocket
{
public:
    XMLConfigInput()
    {
	try {
            setInet4McastSocketAddress(
                atdUtil::Inet4SocketAddress(
                    atdUtil::Inet4Address::getByName(DSM_MULTICAST_ADDR),
                    DSM_SVC_REQUEST_PORT));
        }
        catch(const atdUtil::UnknownHostException& e) {
        }

        setRequestNumber(XML_CONFIG);
    }

protected:
};

}

#endif
