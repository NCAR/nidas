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
        SensorHandler* handler):
    _name(),_socket(sock),_devname(),_sensor(0),_serSensor(0),
    _input(), _nullTerminated(false),_handler(handler)
{
    setName(sock->getRemoteSocketAddress().toAddressString());
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
    if (_sensor) _sensor->getRawSampleSource()->removeSampleClient(this);
    _sensor = 0;

    if (_socket->getFd() >= 0) {
#if POLLING_METHOD == POLL_EPOLL_ET || POLLING_METHOD == POLL_EPOLL_LT
        if (::epoll_ctl(_handler->getEpollFd(),EPOLL_CTL_DEL,_socket->getFd(),NULL) < 0) {
            n_u::IOException e(getName(),"EPOLL_CTL_DEL",errno);
            _socket->close();
            throw e;
        }
#endif
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

void RemoteSerialConnection::setSensor(CharacterSensor* val)
	throw(n_u::IOException)
{
    _sensor = val;
    _serSensor = dynamic_cast<SerialSensor*>(val);

    ostringstream ost;

    setName(_socket->getRemoteSocketAddress().toAddressString() + ": " + _sensor->getName());

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

    ost << _sensor->getBackslashedMessageSeparator() << endl;
    _socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << _sensor->getMessageSeparatorAtEOM() << endl;
    _socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << _sensor->getMessageLength() << endl;
    _socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    ost << "prompted=" << boolalpha << _sensor->isPrompted() << endl;
    _socket->send(ost.str().c_str(),ost.str().size());
    ost.str("");

    _nullTerminated = _sensor->getNullTerminated();
    // cerr << "nullTerminated=" << _nullTerminated << endl;

    val->getRawSampleSource()->addSampleClient(this);

    // this connection is ready for polling
#if POLLING_METHOD == POLL_EPOLL_ET
    _socket->setNonBlocking(true);
#endif

#if POLLING_METHOD == POLL_EPOLL_ET || POLLING_METHOD == POLL_EPOLL_LT
    epoll_event event = epoll_event();

#if POLLING_METHOD == POLL_EPOLL_ET
    event.events = EPOLLIN | EPOLLET;
#else
    event.events = EPOLLIN;
#endif

    event.data.ptr = (Polled*)this;
    if (::epoll_ctl(_handler->getEpollFd(),EPOLL_CTL_ADD,_socket->getFd(),&event) < 0)
        throw n_u::IOException(getName(),"EPOLL_CTL_ADD",errno);
#endif
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
		if (_sensor) {
		    string msg;
		    try {
			_sensor->togglePrompting();
			msg = string("dsm: prompting = ") +
			    (_sensor->isPrompting() ? "ON" : "OFF") + "\r\n";
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
	    string msg = _sensor->getName() + ": " + e.what();
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

#if POLLING_METHOD == POLL_EPOLL_ET
bool
#else
void
#endif
RemoteSerialConnection::handlePollEvents(uint32_t events) throw()
{
#if POLLING_METHOD == POLL_EPOLL_ET
    bool exhausted = false;
#endif

    if (events & N_POLLRDHUP) {
        PLOG(("%s: POLLRDHUP",getName().c_str()));
        _handler->scheduleClose(this);
#if POLLING_METHOD == POLL_EPOLL_ET
        return true;
#else
        return;
#endif
    }
    if (events & (N_POLLERR | N_POLLHUP)) {
        PLOG(("%s: POLLERR or POLLHUP", getName().c_str()));
        _handler->scheduleClose(this);
#if POLLING_METHOD == POLL_EPOLL_ET
        return true;
#else
        return;
#endif
    }
    if (events & N_POLLIN) {
        try {
            char buffer[512];
            size_t l = _socket->recv(buffer,sizeof(buffer));
#if POLLING_METHOD == POLL_EPOLL_ET
            exhausted = l < sizeof(buffer);
#endif
            // cerr << "RemoteSerialConnection read " << i << " bytes" << endl;
            // n_u::Logger::getInstance()->log(
                //     LOG_INFO,"RemoteSerialConnection() read %d bytes",i);
            // if (i == 0) throw n_u::EOFException("rserial socket","read");

            // we're not handling the situation of a write() not writing
            // all the data.
            if (_sensor) {
                string output = doEscCmds(string(buffer,l));
                if (output.length() > 0) {
                    // nlTocrnl(output);

                    // A sensor's open() method may do some initialization,
                    // and that might fail if the sensor is not responding
                    // as expected.  After the open fails, it might be
                    // rescheduled to be reopened.  Therefore this sensor's
                    // file descriptor may not be open. If that is the case
                    // just log the error and keep trying.
                    if (_sensor->getWriteFd() >= 0) {
                        try {
                            _sensor->write(output.c_str(),output.length());
                        }
                        catch(const n_u::IOException & ioe) {
                            ILOG(("%s: %s", getName().c_str(),ioe.what()));
                        }
                    }
                }
            }
        }
        catch(const n_u::EOFException & ioe) {
            ILOG(("%s: %s", getName().c_str(),ioe.what()));
            _handler->scheduleClose(this);
#if POLLING_METHOD == POLL_EPOLL_ET
            exhausted = true;
#endif
        }
        catch(const n_u::IOException & ioe) {
            ILOG(("%s: %s", getName().c_str(),ioe.what()));
            _handler->scheduleClose(this);
#if POLLING_METHOD == POLL_EPOLL_ET
            exhausted = true;
#endif
        }
    }
#if POLLING_METHOD == POLL_EPOLL_ET
    return exhausted;
#endif
}
