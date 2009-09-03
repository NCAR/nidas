/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/RemoteSerialConnection.h>
#include <nidas/dynld/DSMSerialSensor.h>

#include <nidas/util/Logger.h>

#include <unistd.h>

#include <ostream>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

RemoteSerialConnection::RemoteSerialConnection(n_u::Socket* sock):
	socket(sock),charSensor(0),nullTerminated(false)
{
    setName(sock->getRemoteSocketAddress().toString());
}

RemoteSerialConnection::~RemoteSerialConnection()
{

    if (charSensor) charSensor->getRawSampleSource()->removeSampleClient(this);
    socket->close();
    delete socket;
}

void RemoteSerialConnection::close() throw(n_u::IOException)
{
    if (charSensor) charSensor->getRawSampleSource()->removeSampleClient(this);
    charSensor = 0;
    socket->close();
}

void RemoteSerialConnection::readSensorName() throw(n_u::IOException)
{
    /*
    * the first message will have the device name in it.
    */
    char dev[128];
    int n = socket->recv(dev, (sizeof dev) - 1, 0);

    dev[n] = 0;
    char* nl = strchr(dev,'\n');
    if (nl) *nl = 0;

    devname = string(dev);
    socket->setNonBlocking(true);
}

void RemoteSerialConnection::sensorNotFound()
	throw(n_u::IOException)
{
    ostringstream ost;
    ost << "ERROR: sensor " << getSensorName() << " not found.";
    ost << endl;
    string msg = ost.str();
    socket->send(msg.c_str(),msg.length());
    close();
    return;
}


void RemoteSerialConnection::setDSMSensor(DSMSensor* val)
	throw(n_u::IOException)
{

    charSensor = dynamic_cast<CharacterSensor*>(val);
    DSMSerialSensor* serSensor = dynamic_cast<DSMSerialSensor*>(charSensor);

    ostringstream ost;

    if(!charSensor) {
	ost << val->getName() << " is not a CharacterSensor";
	n_u::Logger::getInstance()->log(LOG_INFO,"%s",ost.str().c_str());

	ost << endl;
	string msg = "ERROR: " + ost.str();
	socket->send(msg.c_str(),msg.size());
	close();
	return;
    }

    setName(socket->getRemoteSocketAddress().toString() + ": " + charSensor->getName());

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
    else {
	ost << 9999 << ' ' <<
	    "even" << ' ' <<
	    8 << ' ' <<
	    1 << endl;
	socket->send(ost.str().c_str(),ost.str().size());
	ost.str("");
    }

    ost << charSensor->getBackslashedMessageSeparator() << endl;
    socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << charSensor->getMessageSeparatorAtEOM() << endl;
    socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << charSensor->getMessageLength() << endl;
    socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << "prompted=" << boolalpha << charSensor->isPrompted() << endl;
    socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    nullTerminated = charSensor->getNullTerminated();
    // cerr << "nullTerminated=" << nullTerminated << endl;

    val->getRawSampleSource()->addSampleClient(this);
}

/**
 * Receive a sample from the DSMSensor, write data portion to socket.
 */
bool RemoteSerialConnection::receive(const Sample* s) throw()
{
    size_t slen = s->getDataLength();
    if (nullTerminated && ((const char*) s->getConstVoidDataPtr())[slen-1] == '\0') {
	// cerr << "nullTerminated=" << nullTerminated << " slen=" << slen << endl;
	slen--;
    }
    try {
	socket->send(s->getConstVoidDataPtr(), slen);
    }
    catch (const n_u::IOException& e)
    {
	try {
	    close();
	}
	catch (const n_u::IOException& e2) {}
    }
    return true;
}

void RemoteSerialConnection::nlTocrnl(string& input)
{
    string::size_type pos = 0;
    string::size_type nl;
    while (pos < input.length() &&
    	(nl = input.find('\n',pos)) != string::npos) {
	// cerr << "pos=" << pos << " nl=" << nl << endl;
	input = input.substr(0,nl) + '\r' + input.substr(nl);
	pos = nl + 2;
    }
}

string RemoteSerialConnection::doEscCmds(const string& inputstr)
	throw(n_u::IOException)
{
    input += inputstr;
    string output;
    // cerr << "input.length() = " << input.length() << endl;
    string::size_type esc = string::npos;
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
			socket->send(msg.c_str(),msg.size());
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
		    // if (charSensor) charSensor->setBaudRate(baud);
		    break;
		}
	    case 'p':		// toggle prompting
		// remove escape sequence from buffer
		input = input.substr(2);
		if (charSensor) {
		    string msg;
		    try {
			charSensor->togglePrompting();
			msg = string("dsm: prompting = ") +
			    (charSensor->isPrompting() ? "ON" : "OFF") + "\r\n";
		    }
		    catch (const n_u::IOException& e)
		    {
			msg = e.what();
		    }
		    socket->send(msg.c_str(),msg.size());
		}
		// cerr << "toggle prompting" << endl;
		break;
	    default:		// unrecognized escape seq, send it on
	        output += input.substr(0,2);
		input = input.substr(2);
		break;
	    }
	}
	catch (const n_u::IOException& e) {
	    string msg = charSensor->getName() + ": " + e.what();
	    n_u::Logger::getInstance()->log(LOG_WARNING,
		"%s",msg.c_str());
	    msg.append("\r\n");
	    socket->send(msg.c_str(),msg.size());
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
void RemoteSerialConnection::read() throw(n_u::IOException) 
{
    char buffer[512];
    ssize_t i = socket->recv(buffer,sizeof(buffer));
    // cerr << "RemoteSerialConnection read " << i << " bytes" << endl;
    // n_u::Logger::getInstance()->log(
	//     LOG_INFO,"RemoteSerialConnection() read %d bytes",i);
    // if (i == 0) throw n_u::EOFException("rserial socket","read");

    // we're not handling the situation of a write() not writing
    // all the data.
    if (charSensor) {
	string output = doEscCmds(string(buffer,i));
	if (output.length() > 0) {
	    // nlTocrnl(output);
	    charSensor->write(output.c_str(),output.length());
	}
    }
}

