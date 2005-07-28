/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <unistd.h>

#include <RemoteSerialConnection.h>

#include <atdUtil/Logger.h>

#include <ostream>

using namespace dsm;
using namespace std;

RemoteSerialConnection::~RemoteSerialConnection()
{

    atdUtil::Logger::getInstance()->log(LOG_INFO,"~RemoteSerialConnection()");
    if (sensor) sensor->removeRawSampleClient(this);
    socket->close();
    delete socket;
}

void RemoteSerialConnection::setDSMSensor(DSMSensor* val)
	throw(atdUtil::IOException) {
    if (!val) {
	if (sensor) sensor->removeRawSampleClient(this);
	sensor = 0;
	return;
    }
    sensor = dynamic_cast<DSMSerialSensor*>(val);

    ostringstream ost;

    if(!sensor) {
	ost << val->getName() << " is not a DSMSerialSensor";
	atdUtil::Logger::getInstance()->log(LOG_INFO,"%s",ost.str().c_str());

	ost << endl;
	string msg = "ERROR: " + ost.str();
	socket->send(msg.c_str(),msg.size());
	return;
    }

    ost << "OK" << endl;
    socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << sensor->getBaudRate() << ' ' << sensor->getParityString() <<
    	' ' << sensor->getDataBits() << ' ' << sensor->getStopBits() << endl;
    socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << sensor->getMessageSeparator() << endl;
    socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << sensor->getMessageSeparatorAtEOM() << endl;
    socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << sensor->getMessageLength() << endl;
    socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    val->addRawSampleClient(this);
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
	setDSMSensor(0);
    }
    return true;
}

/**
 * Read data from socket, write to DSMSensor.
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

