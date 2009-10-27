/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/
#ifndef NIDAS_CORE_SOCKETIODEVICE_H
#define NIDAS_CORE_SOCKETIODEVICE_H

#include <nidas/core/IODevice.h>
#include <nidas/util/Socket.h>
#include <nidas/util/ParseException.h>

#include <memory> // auto_ptr<>

namespace nidas { namespace core {

/**
 * A base class for device IO using UDP and TCP socket 
 */
class SocketIODevice : public IODevice {

public:

    /**
     * Create a SocketIODevice.  No IO operations to the sensor
     * are performed in the constructor (hence no IOExceptions).
     */
    SocketIODevice();

    virtual ~SocketIODevice();

    /**
    * open the socket.
    */
    virtual void open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    /*
    * Perform an ioctl on the device. Not necessary for a socket,
    * and will throw an IOException.
    */
    void ioctl(int request, void* buf, size_t len) throw(nidas::util::IOException)
    {
        throw nidas::util::IOException(getName(),
		"ioctl","not supported on SocketIODevice");
    }

    static void parseAddress(const std::string& name, int& addrtype,std::string& hostname,
        int& port) throw(nidas::util::ParseException);

protected:

    /**
     * The type of the destination address, AF_INET or AF_UNIX.
     */
    int addrtype;	

    /**
     * Destination host name from sensor name.
     */
    std::string desthost;

    /**
     * Port number that is parsed from sensor name.
     */
    int destport;

    /**
     * The destination socket address.
     */
    std::auto_ptr<nidas::util::SocketAddress> sockAddr;

};

}}	// namespace nidas namespace core

#endif
