// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
#include <nidas/core/SerialSensor.h>
#include <nidas/core/SensorHandler.h>

#include <nidas/util/Logger.h>

#include <unistd.h>
#include <sys/epoll.h>

#include <ostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

RemoteSerialConnection::RemoteSerialConnection(n_u::Socket* sock,
        SensorHandler* handler) throw(n_u::IOException):
    _name(),_socket(sock),_devname(),_charSensor(0),_serSensor(0),
    _input(), _nullTerminated(false),_handler(handler)
{
    setName(sock->getRemoteSocketAddress().toString());

    epoll_event event;

#ifdef TEST_EDGE_TRIGGERED_EPOLL
    event.events = EPOLLIN | EPOLLET;
#else
    event.events = EPOLLIN;
#endif

    event.data.ptr = (EpollFd*)this;
    if (::epoll_ctl(_handler->getEpollFd(),EPOLL_CTL_ADD,_socket->getFd(),&event) < 0)
        throw n_u::IOException(getName(),"EPOLL_CTL_ADD",errno);
}

RemoteSerialConnection::~RemoteSerialConnection()
{

    try {
        close();
    }
    catch (const n_u::IOException& e) {
	NLOG(("%s",e.what()));
    }
    delete _socket;
}

void RemoteSerialConnection::close() throw(n_u::IOException)
{
    if (_charSensor) _charSensor->getRawSampleSource()->removeSampleClient(this);
    _charSensor = 0;

    if (_socket->getFd() >= 0) {
        if (::epoll_ctl(_handler->getEpollFd(),EPOLL_CTL_DEL,_socket->getFd(),NULL) < 0) {
            n_u::IOException e(getName(),"EPOLL_CTL_DEL",errno);
            _socket->close();
            throw e;
        }
        _socket->close();
    }
}

void RemoteSerialConnection::readSensorName() throw(n_u::IOException)
{
    /*
    * the first message will have the device name in it.
    */
    char dev[128];
    int n = _socket->recv(dev, (sizeof dev) - 1, 0);

    dev[n] = 0;
    char* nl = strchr(dev,'\n');
    if (nl) *nl = 0;

    _devname = string(dev);
    _socket->setNonBlocking(true);
}

void RemoteSerialConnection::sensorNotFound()
	throw(n_u::IOException)
{
    ostringstream ost;
    ost << "ERROR: sensor " << getSensorName() << " not found.";
    ost << endl;
    string msg = ost.str();
    _socket->send(msg.c_str(),msg.length());
    WLOG(("%s",msg.c_str()));
    return;
}

void RemoteSerialConnection::setDSMSensor(DSMSensor* val)
	throw(n_u::IOException)
{
    _charSensor = dynamic_cast<CharacterSensor*>(val);
    _serSensor = dynamic_cast<SerialSensor*>(_charSensor);

    ostringstream ost;

    if(!_charSensor) {
	ost << val->getName() << " is not a CharacterSensor";
	n_u::Logger::getInstance()->log(LOG_INFO,"%s",ost.str().c_str());

	ost << endl;
	string msg = "ERROR: " + ost.str();
	_socket->send(msg.c_str(),msg.size());
	return;
    }

    setName(_socket->getRemoteSocketAddress().toString() + ": " + _charSensor->getName());

    ost << "OK" << endl;
    _socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    if (_serSensor) {
	ost << _serSensor->getTermios().getBaudRate() << ' ' <<
	    _serSensor->getTermios().getParityString() << ' ' <<
	    _serSensor->getTermios().getDataBits() << ' ' <<
	    _serSensor->getTermios().getStopBits() << endl;
	_socket->send(ost.str().c_str(),ost.str().size());
	ost.str("");
    }
    else {
	ost << 9999 << ' ' <<
	    "even" << ' ' <<
	    8 << ' ' <<
	    1 << endl;
	_socket->send(ost.str().c_str(),ost.str().size());
	ost.str("");
    }

    ost << _charSensor->getBackslashedMessageSeparator() << endl;
    _socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << _charSensor->getMessageSeparatorAtEOM() << endl;
    _socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << _charSensor->getMessageLength() << endl;
    _socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << "prompted=" << boolalpha << _charSensor->isPrompted() << endl;
    _socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    _nullTerminated = _charSensor->getNullTerminated();
    // cerr << "nullTerminated=" << _nullTerminated << endl;

    val->getRawSampleSource()->addSampleClient(this);

}

/**
 * Receive a sample from the DSMSensor, write data portion to socket.
 */
bool RemoteSerialConnection::receive(const Sample* s) throw()
{
    size_t slen = s->getDataLength();
    if (_nullTerminated && ((const char*) s->getConstVoidDataPtr())[slen-1] == '\0') {
	// cerr << "nullTerminated=" << _nullTerminated << " slen=" << slen << endl;
	slen--;
    }
    try {
	_socket->send(s->getConstVoidDataPtr(), slen);
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
    _input += inputstr;
    string output;
    // cerr << "_input.length() = " << _input.length() << endl;
    string::size_type esc = string::npos;
    bool done = false;

    for ( ; !done && _input.length() > 0 &&
    	(esc = _input.find('\x1B')) != string::npos; ) {
	// cerr << "found ESC, esc=" << esc << endl;
	output += _input.substr(0,esc);
	_input = _input.substr(esc);
	if (_input.length() == 1) break;		// only ESC
	try {
	    switch (_input[1]) {
	    case '\x1B':		// double escape sequence
		output += _input[1]; 	// send single escape
		_input = _input.substr(2);
		break;
	    case 'b':			// change baud rate
	    case 'B':			// change baud rate
		{
		    size_t idx = 2;
		    for ( ; idx < _input.length() && isdigit(_input[idx]);
		    	idx++);
		    // must find a non-digit, otherwise keep reading
		    if (idx == _input.length()) {
			done = true;
		        break;
		    }

		    if (idx == 2) {	// no digits
			string msg = "Invalid baud rate: \"" +
			    _input.substr(2) + "\"\r\n" ;
			_socket->send(msg.c_str(),msg.size());
			output += _input.substr(0,idx);
			_input = _input.substr(idx);
			break;
		    }
		    int baud;
		    sscanf(_input.substr(2,idx-2).c_str(),"%d",&baud);
		    while (idx < _input.length() &&
		    	(_input[idx] == ' ' ||
			 _input[idx] == '\n' ||
			 _input[idx] == '\r')) idx++;
		    _input = _input.substr(idx);
		    cerr << "baud=" << baud << endl;
		    if (_serSensor) {
                        _serSensor->termios().setBaudRate(baud);
                        _serSensor->applyTermios();
                    }
		    break;
		}
	    case 'p':		// toggle prompting
		// remove escape sequence from buffer
		_input = _input.substr(2);
		if (_charSensor) {
		    string msg;
		    try {
			_charSensor->togglePrompting();
			msg = string("dsm: prompting = ") +
			    (_charSensor->isPrompting() ? "ON" : "OFF") + "\r\n";
		    }
		    catch (const n_u::IOException& e)
		    {
			msg = e.what();
		    }
		    _socket->send(msg.c_str(),msg.size());
		}
		// cerr << "toggle prompting" << endl;
		break;
	    default:		// unrecognized escape seq, send it on
	        output += _input.substr(0,2);
		_input = _input.substr(2);
		break;
	    }
	}
	catch (const n_u::IOException& e) {
	    string msg = _charSensor->getName() + ": " + e.what();
	    n_u::Logger::getInstance()->log(LOG_WARNING,
		"%s",msg.c_str());
	    msg.append("\r\n");
	    _socket->send(msg.c_str(),msg.size());
	}
    }
    if (esc == string::npos) {
        output += _input;
	_input.clear();
    }
    // cerr << "output.length() = " << output.length() << endl;
    return output;
}

void RemoteSerialConnection::handleEpollEvents(uint32_t events) throw()
{
#ifdef EPOLLRDHUP
    if (events & EPOLLRDHUP) {
        PLOG(("%s: EPOLLRDHUP",getName().c_str()));
        _handler->removeRemoteSerialConnection(this);
    }
#endif
    if (events & (EPOLLERR | EPOLLHUP)) {
        PLOG(("%s: EPOLLERR or EPOLLHUP", getName().c_str()));
        _handler->removeRemoteSerialConnection(this);
    }
    if (events & EPOLLIN) {
        try {
            char buffer[512];
            ssize_t i = _socket->recv(buffer,sizeof(buffer));
            // cerr << "RemoteSerialConnection read " << i << " bytes" << endl;
            // n_u::Logger::getInstance()->log(
                //     LOG_INFO,"RemoteSerialConnection() read %d bytes",i);
            // if (i == 0) throw n_u::EOFException("rserial socket","read");

            // we're not handling the situation of a write() not writing
            // all the data.
            if (_charSensor) {
                string output = doEscCmds(string(buffer,i));
                if (output.length() > 0) {
                    // nlTocrnl(output);
                    _charSensor->write(output.c_str(),output.length());
                }
            }
        }
        catch(const n_u::EOFException & ioe) {
            ILOG(("%s: %s", getName().c_str(),ioe.what()));
            _handler->removeRemoteSerialConnection(this);
            return;
        }
        catch(const n_u::IOException & ioe) {
            ILOG(("%s: %s", getName().c_str(),ioe.what()));
            _handler->removeRemoteSerialConnection(this);
            return;
        }
    }
}
