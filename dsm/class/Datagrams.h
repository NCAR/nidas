/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_DATAGRAMS_H
#define DSM_DATAGRAMS_H

#include <atdUtil/DatagramPacket.h>

#define DSM_MULTICAST_PORT 50000
#define DSM_MULTICAST_ADDR "239.0.0.10"

namespace dsm {

/**
 * Datagram that is multicast by a DSM when is is ready to receive
 * its configuration.
 */
class ConfigDatagram: public atdUtil::DatagramPacket {
public:
    ConfigDatagram() : atdUtil::DatagramPacket((char*)&contents,
    	sizeof(contents))
    {
        setLength(sizeof(contents));
    }

    /**
     * What port is the DSM listening on for its configuration?
     */
    int getDSMListenPort() const { return ntohs(contents.listenPort); }

    void setDSMListenPort(int val) { contents.listenPort = htons(val); }

    /**
     * How patiently has the DSM been waiting?
     */
    int getNumMulticasts() const { return ntohl(contents.numMulticasts); }
    void setNumMulticasts(int val) { contents.numMulticasts = htonl(val); }
    
protected:
    struct contents {
	/**
	 * TCP stream socket port that the dsm is listening on.
	 * Stored in "network", big-endian order.
	 */
	unsigned short listenPort;

	/**
	 * How many multicasts has it sent.
	 * Stored in "network", big-endian order.
	 */
	int numMulticasts;
    } contents;

};

}

#endif
