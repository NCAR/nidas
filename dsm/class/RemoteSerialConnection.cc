/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <unistd.h>

#include <RemoteSerialConnection.h>

using namespace dsm;

RemoteSerialConnection::~RemoteSerialConnection()
{
    if (port) port->removeSampleClient(this);
    ::close(fd);
}

/**
 * Receive a sample from the DSMSensor, write data portion to fd.
 */
bool RemoteSerialConnection::receive(const Sample* s)
	    throw(SampleParseException,atdUtil::IOException)
{
    if (::write(fd,s->getConstVoidDataPtr(), s->getDataLength()) < 0)
	setPort(0);
    return true;
}

/**
 * Read data from file descriptor, write to DSMSensor.
 */
void RemoteSerialConnection::read() throw(atdUtil::IOException) 
{
    char buffer[512];
    ssize_t i = ::read(fd,buffer,sizeof(buffer));
    if (i <= 0) {
        setPort(0);
	if (i == 0) throw atdUtil::EOFException("rserial socket","read");
	if (i < 0) throw atdUtil::IOException("rserial socket","read",errno);
    }

    // don't handle situation of writing less than i bytes
    // to serial port
    if (port) port->write(buffer,i);
}

