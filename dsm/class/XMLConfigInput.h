/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifndef DSM_XMLCONFIGINPUT_H
#define DSM_XMLCONFIGINPUT_H

#include <atdUtil/McSocket.h>
#include <Datagrams.h>

namespace dsm {

class XMLConfigInput: public atdUtil::McSocketRequester
{
public:
    XMLConfigInput()
    {
	try {
            setInet4McastSocketAddress(
                atdUtil::Inet4SocketAddress(
                    atdUtil::Inet4Address::getByName(DSM_MULTICAST_ADDR),
                    DSM_MULTICAST_PORT));
        }
        catch(const atdUtil::UnknownHostException& e) {
        }

        setPseudoPort(XML_CONFIG);
    }

protected:
};

}

#endif
