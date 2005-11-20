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
#include <DSMSerialSensor.h>
#include <SocketSensor.h>

#include <atdUtil/Logger.h>

#include <ostream>

using namespace dsm;
using namespace std;

RemoteSerialConnection::RemoteSerialConnection(atdUtil::Socket* sock,
	const std::string& d):
	socket(sock),devname(d),sensor(0),nullTerminated(false)
{
}

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
    DSMSerialSensor* serSensor = 0;
    SocketSensor* sockSensor = 0;
    msgSensor = 0;

    sensor = serSensor = dynamic_cast<DSMSerialSensor*>(val);
    msgSensor = serSensor;

    if (!sensor) {
    	sensor = sockSensor = dynamic_cast<SocketSensor*>(val);
	msgSensor = sockSensor;
    }

    ostringstream ost;

    if(!sensor) {
	ost << val->getName() << " is not a DSMSerialSensor or SocketSensor";
	atdUtil::Logger::getInstance()->log(LOG_INFO,"%s",ost.str().c_str());

	ost << endl;
	string msg = "ERROR: " + ost.str();
	socket->send(msg.c_str(),msg.size());
	return;
    }

    ost << "OK" << endl;
    socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    if (serSensor) {
	ost << serSensor->getBaudRate() << ' ' <<
	    serSensor->getParityString() << ' ' <<
	    serSensor->getDataBits() << ' ' <<
	    serSensor->getStopBits() << endl;
	socket->send(ost.str().c_str(),ost.str().size());
	ost.str("");
    }

    ost << msgSensor->getBackslashedMessageSeparator() << endl;
    socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << msgSensor->getMessageSeparatorAtEOM() << endl;
    socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << msgSensor->getMessageLength() << endl;
    socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    nullTerminated = msgSensor->isNullTerminated();
    cerr << "nullTerminated=" << nullTerminated << endl;

    val->addRawSampleClient(this);
}

/**
 * Receive a sample from the DSMSensor, write data portion to socket.
 */
bool RemoteSerialConnection::receive(const Sample* s) throw()
{
    size_t slen = s->getDataLength();
    if (nullTerminated && ((const char*) s->getConstVoidDataPtr())[slen-1] == '\0') {
	cerr << "nullTerminated=" << nullTerminated << " slen=" << slen << endl;
	slen--;
    }
    try {
	socket->send(s->getConstVoidDataPtr(), slen);
    }
    catch (const atdUtil::IOException& e)
    {
	setDSMSensor(0);
    }
    return true;
}

void RemoteSerialConnection::nlTocrnl(string& input)
{
    size_t pos = 0;
    size_t nl;
    while (pos < input.length() &&
    	(nl = input.find('\n',pos)) != string::npos) {
	// cerr << "pos=" << pos << " nl=" << nl << endl;
	input = input.substr(0,nl) + '\r' + input.substr(nl);
	pos = nl + 2;
    }
}

string RemoteSerialConnection::doEscCmds(const string& inputstr)
	throw(atdUtil::IOException)
{
    input += inputstr;
    string output;
    // cerr << "input.length() = " << input.length() << endl;
    size_t esc = string::npos;
    bool done = false;

    for ( ; !done && input.length() > 0 &&
    	(esc = input.find('\x1B')) != string::npos; ) {
	// cerr << "found ESC, esc=" << esc << endl;
	output += input.substr(0,esc);
	input = input.substr(esc);
	if (input.length() == 1) break;		// only ESC
	try {
	    switch (input[1]) {
	    case '\x1B':		// double escape sequence
		output += input[1]; 	// send single escape
		input = input.substr(2);
		break;
	    case 'b':			// change baud rate
	    case 'B':			// change baud rate
		{
		    size_t idx = 2;
		    for ( ; idx < input.length() && isdigit(input[idx]);
		    	idx++);
		    // must find a non-digit, otherwise keep reading
		    if (idx == input.length()) {
			done = true;
		        break;
		    }

		    if (idx == 2) {	// no digits
			string msg = "Invalid baud rate: \"" +
			    input.substr(2) + "\"\r\n" ;
			try {
			    socket->send(msg.c_str(),msg.size());
			}
			catch (const atdUtil::IOException& e)
			{
			    setDSMSensor(0);
			}
			output += input.substr(0,idx);
			input = input.substr(idx);
			break;
		    }
		    int baud;
		    sscanf(input.substr(2,idx-2).c_str(),"%d",&baud);
		    while (idx < input.length() &&
		    	(input[idx] == ' ' ||
			 input[idx] == '\n' ||
			 input[idx] == '\r')) idx++;
		    input = input.substr(idx);
		    cerr << "baud=" << baud << endl;
		    // if (sensor) sensor->setBaudRate(baud);
		    break;
		}
	    case 'p':		// toggle prompting
		// remove escape sequence from buffer
		input = input.substr(2);
		if (sensor) {
		    string msg;
		    try {
			msgSensor->togglePrompting();
			msg = string("dsm: prompting = ") +
			    (msgSensor->isPrompting() ? "ON" : "OFF") + "\r\n";
		    }
		    catch (const atdUtil::IOException& e)
		    {
			msg = e.what();
		    }
		    try {
			socket->send(msg.c_str(),msg.size());
		    }
		    catch (const atdUtil::IOException& e)
		    {
			setDSMSensor(0);
		    }
		}
		// cerr << "toggle prompting" << endl;
		break;
	    default:		// unrecognized escape seq, send it on
	        output += input.substr(0,2);
		input = input.substr(2);
		break;
	    }
	}
	catch (const atdUtil::IOException& e) {
	    string msg = sensor->getName() + ": " + e.what();
	    atdUtil::Logger::getInstance()->log(LOG_WARNING,
		"%s",msg.c_str());
	    msg.append("\r\n");
	    try {
		socket->send(msg.c_str(),msg.size());
	    }
	    catch (const atdUtil::IOException& e)
	    {
		setDSMSensor(0);
	    }
	}
    }
    if (esc == string::npos) {
        output += input;
	input.clear();
    }
    // cerr << "output.length() = " << output.length() << endl;
    return output;
}

/**
 * Read data from socket, write to DSMSensor.
 */
void RemoteSerialConnection::read() throw(atdUtil::IOException) 
{
    char buffer[512];
    ssize_t i = socket->recv(buffer,sizeof(buffer));
    // cerr << "RemoteSerialConnection read " << i << " bytes" << endl;
    // atdUtil::Logger::getInstance()->log(
	//     LOG_INFO,"RemoteSerialConnection() read %d bytes",i);
    // if (i == 0) throw atdUtil::EOFException("rserial socket","read");

    // were not handling the situation of a write() not writing
    // all the data.
    if (sensor) {
	string output = doEscCmds(string(buffer,i));
	if (output.length() > 0) {
	    nlTocrnl(output);
	    sensor->write(output.c_str(),output.length());
	}
    }
}

