/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

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
    if (sensor) sensor->removeSampleClient(this);
    socket->close();
    delete socket;
}

/**
 * Receive a sample from the DSMSensor, write data portion to fd.
 */
bool RemoteSerialConnection::receive(const Sample* s) throw()
{
    try {
	socket->send(s->getConstVoidDataPtr(), s->getDataLength());
    }
    catch (const atdUtil::IOException& e)
    {
	setSensor(0);
    }
    return true;
}

/**
 * Read data from file descriptor, write to DSMSensor.
 */
void RemoteSerialConnection::read() throw(atdUtil::IOException) 
{
    char buffer[512];
    ssize_t i = socket->recv(buffer,sizeof(buffer));
    if (i == 0) throw atdUtil::EOFException("rserial socket","read");

    // don't handle situation of writing less than i bytes
    // to serial sensor
    if (sensor) sensor->write(buffer,i);
}

