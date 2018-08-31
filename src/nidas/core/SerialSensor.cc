// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "SerialSensor.h"
#include "TCPSocketIODevice.h"
#include "UDPSocketIODevice.h"

#ifdef HAVE_BLUETOOTH_RFCOMM_H
#include "BluetoothRFCommSocketIODevice.h"
#endif

#include "Looper.h"
#include "Prompt.h"

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <locale>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

SerialSensor::SerialSensor():
    _workingPortConfig(), _portTypeList(), _baudRateList(), _serialWordSpecList(),
	_defaultPortConfig(), _serialDevice(0), _prompters(), _prompting(false)
{
    setDefaultMode(O_RDWR);
    _workingPortConfig.termios.setRaw(true);
    _workingPortConfig.termios.setRawLength(1);
    _workingPortConfig.termios.setRawTimeout(0);
}

SerialSensor::SerialSensor(const PortConfig& rInitPortConfig):
		_workingPortConfig(rInitPortConfig), _portTypeList(), _baudRateList(), _serialWordSpecList(),
		_defaultPortConfig(rInitPortConfig), _serialDevice(0), _prompters(), _prompting(false)
{
}

SerialSensor::~SerialSensor()
{
    list<Prompter*>::const_iterator pi = _prompters.begin();
    for (; pi != _prompters.end(); ++pi) delete *pi;
}

SampleScanner* SerialSensor::buildSampleScanner()
	throw(n_u::InvalidParameterException)
{
    SampleScanner* scanr = CharacterSensor::buildSampleScanner();
    DLOG(("%s: usec/byte=%d",getName().c_str(),getUsecsPerByte()));
    scanr->setUsecsPerByte(getUsecsPerByte());
    return scanr;
}

IODevice* SerialSensor::buildIODevice() throw(n_u::IOException)
{
    IODevice* device = CharacterSensor::buildIODevice();

    // Did we get the default UnixIODevice from Character Sensor?
    if (!dynamic_cast<TCPSocketIODevice*>(device)
        && !dynamic_cast<UDPSocketIODevice*>(device)
#ifdef HAVE_BLUETOOTH_RFCOMM_H
        && !dynamic_cast<BluetoothRFCommSocketIODevice*>(device)
#endif
       ) {
        // yes, meaning it didn't find a non-serial port device.
        delete device;
        device = 0;
        DLOG(("SerialSensor: Instantiating a SerialPortIODevice on device ") << getDeviceName());
        // auto-config may have passed down a default port config. No harm if a non-auto-config sensor
        // does not pass down the port config, as later it will be filled in by the XML.
        _serialDevice = new SerialPortIODevice(getDeviceName(), _workingPortConfig);
        device = _serialDevice;
        // !!!TODO check for _serialDevice != null!!!
    }

    return device;
}

int SerialSensor::getUsecsPerByte() const
{
    if (_serialDevice) return _serialDevice->getUsecsPerByte();
    return 0;
}

void SerialSensor::open(int flags)
    throw(n_u::IOException,n_u::InvalidParameterException)
{
    flags |= O_NOCTTY;
    CharacterSensor::open(flags);

    serPortFlush();

    doAutoConfig();

    sendInitString();

    // FSSPs set up their message parameters in the sendInitString() method, so we can't check
    // for this problem until now.
    if (getDeviceName().find("usock:") != 0 &&
        getMessageLength() + getMessageSeparator().length() == 0)
            throw n_u::InvalidParameterException(getName(),"message","must specify a message separator or a non-zero message length");

    initPrompting();
}

void SerialSensor::serPortFlush(const int flags) 
{
    // used to hold the port access flags, whether passed in or read from the device.
    // if reading from the device, the device must be open
    int attrFlags = 0;

    if (_serialDevice && getReadFd() && ::isatty(getReadFd())) {
        if (!flags) {
            attrFlags = fcntl(getReadFd(), F_GETFL, 0);
            // TODO: handle error attrFlags < 0
        }
        else {
            attrFlags = flags;
        }

        int accmode = attrFlags & O_ACCMODE;
        if (accmode == O_RDONLY) {
             _serialDevice->flushInput();
             ILOG(("Flushed serial port input on device: ") << getName());
        }
        else if (accmode == O_WRONLY) {
            _serialDevice->flushOutput();
             ILOG(("Flushed serial port output on device: ") << getName());
        }
        else {
            _serialDevice->flushBoth();
             ILOG(("Flushed serial port input and output on device: ") << getName());
        }
    }
}

// void SerialSensor::setMessageParameters(unsigned int len, const string& sep, bool eom)
//     throw(n_u::InvalidParameterException, n_u::IOException)
// {
//     CharacterSensor::setMessageParameters(len,sep,eom);

//     // Note we don't change _termios here.
//     // Termios is set to to raw mode, len=1, in the constructor.
//     // Very old NIDAS code did a _termio.setRawLength() to the
//     // message length, but not any more. I don't think it made
//     // things any more efficient, and may have reduced the accuracy of
//     // time-tagging.
// }

void SerialSensor::close() throw(n_u::IOException)
{
    shutdownPrompting();
    DSMSensor::close();
}

void SerialSensor::setPortConfig(const PortConfig newPortConfig)
{
    if (_serialDevice) {
        _serialDevice->setPortConfig(newPortConfig);
    }

    else {
        NLOG(("SerialSensor::setPortConfig(): device, ") << getName() 
             << (", is trying to set a PortConfig too early, "
                 "or is a newer type serial device such as USB or socket-oriented, "
                 " which has no need of a PortConfig."));
    }
}


PortConfig SerialSensor::getPortConfig() 
{
    static PortConfig dummy;

    if (_serialDevice) {
        return _serialDevice->getPortConfig();
    }
    else {
        NLOG(("SerialSensor::getPortConfig(): device, ") << getName() 
             << (", is trying to obtain a PortConfig too early, "
                 "or is newer type serial device such as USB or socket-oriented, "
                 " which has no need of a PortConfig."));
        return dummy;
    }
}

void SerialSensor::applyTermios() throw(nidas::util::IOException)
{
    if (_serialDevice) {
        _serialDevice->applyTermios();
    }
    else {
        NLOG(("SerialSensor::applyTermios(): device, ") << getName() 
             << (", is trying to apply termios too early, "
                 "or is newer type serial device such as USB or socket-oriented, "
                 " which has no need of a PortConfig."));
    }
}

void SerialSensor::applyPortConfig() 
{
    if (_serialDevice) {
        _serialDevice->applyPortConfig();
    }
    else {
        NLOG(("SerialSensor::applyPortConfig(): device, ") << getName() 
             << (", is trying to apply PortConfig too early, "
                 "or is newer type serial device such as USB or socket-oriented, "
                 " which has no need of a PortConfig."));
    }
}

void SerialSensor::printPortConfig(bool flush)
{
    if (_serialDevice) {
        _serialDevice->printPortConfig();
        if (flush) {
        	std::cerr << std::flush;
        	std::cout << std::flush;
        }
    }
    else {
        NLOG(("SerialSensor::printPortConfig(): device, ") << getName() 
             << (", is trying to print a PortConfig too early, "
                 "or is newer type serial device such as USB or socket-oriented, "
                 " which has no need of a PortConfig."));
    }
}

void SerialSensor::initPrompting() throw(n_u::IOException)
{
    if (isPrompted()) {
        const list<Prompt>& prompts = getPrompts();
        list<Prompt>::const_iterator pi = prompts.begin();
        for (; pi != prompts.end(); ++pi) {
           const Prompt& prompt = *pi;
           Prompter* prompter = new Prompter(this);
           prompter->setPrompt(n_u::replaceBackslashSequences(prompt.getString()));
           prompter->setPromptPeriodMsec((int) rint(MSECS_PER_SEC / prompt.getRate()));
           prompter->setPromptOffsetMsec((int) rint(MSECS_PER_SEC * prompt.getOffset()));

           _prompters.push_back(prompter);
           //addPrompter(n_u::replaceBackslashSequences(pi->getString()), (int) rint(MSECS_PER_SEC / pi->getRate()));
           // cerr << "promptPeriodMsec=" << _promptPeriodMsec << endl;
        }
        startPrompting();
    }
}

void SerialSensor::shutdownPrompting() throw(n_u::IOException)
{
    stopPrompting();
    list<Prompter*>::const_iterator pi = _prompters.begin();
    for (; pi != _prompters.end(); ++pi) delete *pi;
    _prompters.clear();
}

void SerialSensor::startPrompting() throw(n_u::IOException)
{
    if (isPrompted()) {
        list<Prompter*>::const_iterator pi;
        //for (pi = getPrompters().begin(); pi != getPrompters.end(); ++pi) {
        for (pi = _prompters.begin(); pi != _prompters.end(); ++pi) {
            Prompter* prompter = *pi;
            getLooper()->addClient(prompter,
                    prompter->getPromptPeriodMsec(),
                    prompter->getPromptOffsetMsec());
        }
	_prompting = true;
    }
}

void SerialSensor::stopPrompting() throw(n_u::IOException)
{
    if (isPrompted()) {
        list<Prompter*>::const_iterator pi;
        //for (pi = getPrompters().begin(); pi = getPrompters().end(); ++pi) {
        for (pi = _prompters.begin(); pi != _prompters.end(); ++pi) {
            Prompter* prompter = *pi;
            getLooper()->removeClient(prompter);
        }
	_prompting = false;
    }
}

void SerialSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    if (_serialDevice) {

        try {
        ostr << "<td align=left>" << getPortConfig().termios.getBaudRate() <<
            getPortConfig().termios.getParityString().substr(0,1) <<
            getPortConfig().termios.getDataBits() << getPortConfig().termios.getStopBits();
        if (getReadFd() < 0) {
            ostr << ",<font color=red><b>not active</b></font>";
            if (getTimeoutMsecs() > 0)
                ostr << ",timeouts=" << getTimeoutCount();
            ostr << "</td>" << endl;
            return;
        }
        if (getTimeoutMsecs() > 0)
                ostr << ",timeouts=" << getTimeoutCount();
        ostr << "</td>" << endl;
        }
        catch(const n_u::IOException& ioe) {
            ostr << "<td>" << ioe.what() << "</td>" << endl;
            n_u::Logger::getInstance()->log(
            	LOG_ERR, "%s: printStatus: %s",
				getName().c_str(), ioe.what());
        }
    }
}

void SerialSensor::fromDOMElement(
	const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    CharacterSensor::fromDOMElement(node);

    XDOMElement xnode(node);

    if(node->hasAttributes()) {
        // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();

            if (aname == "ID")/*ignore*/;
            else if (aname == "IDREF");
            else if (aname == "class");
            else if (aname == "devicename");
            else if (aname == "id");
            else if (aname == "porttype") {
                string upperAval(aval);
                std::transform(aval.begin(), aval.end(), upperAval.begin(), ::toupper);;
                if (upperAval == "RS232") _workingPortConfig.xcvrConfig.portType = RS232;
                else if (upperAval == "RS422") _workingPortConfig.xcvrConfig.portType = RS422;
                else if (upperAval == "RS485_HALF") _workingPortConfig.xcvrConfig.portType = RS485_HALF;
                else if (upperAval == "RS485_FULL") _workingPortConfig.xcvrConfig.portType = RS485_FULL;
                else throw n_u::InvalidParameterException(
                            string("SerialSensor:") + getName(),
                            aname,aval);
            }
            else if (aname == "termination") {
                if (aval == "NO_TERM") _workingPortConfig.xcvrConfig.termination = NO_TERM;
                else if (aval == "TERM_120_OHM") _workingPortConfig.xcvrConfig.termination = TERM_120_OHM;
                else throw n_u::InvalidParameterException(
                            string("SerialSensor:") + getName(),
                            aname,aval);
            }
            else if (aname == "baud") {
                istringstream ist(aval);
                int val;
                ist >> val;
                if (ist.fail() || !_workingPortConfig.termios.setBaudRate(val))
                    throw n_u::InvalidParameterException(
                        string("SerialSensor:") + getName(), aname,aval);
            }
            else if (aname == "parity") {
            if (aval == "odd") _workingPortConfig.termios.setParity(n_u::Termios::ODD);
            else if (aval == "even") _workingPortConfig.termios.setParity(n_u::Termios::EVEN);
            else if (aval == "none") _workingPortConfig.termios.setParity(n_u::Termios::NONE);
            else throw n_u::InvalidParameterException(
                string("SerialSensor:") + getName(),
                aname,aval);
            }
            else if (aname == "databits") {
                istringstream ist(aval);
                int val;
                ist >> val;
                if (ist.fail())
                    throw n_u::InvalidParameterException(
                    string("SerialSensor:") + getName(),
                        aname, aval);
                _workingPortConfig.termios.setDataBits(val);
            }
            else if (aname == "stopbits") {
                istringstream ist(aval);
                int val;
                ist >> val;
                if (ist.fail())
                    throw n_u::InvalidParameterException(
                    string("SerialSensor:") + getName(),
                        aname, aval);
                _workingPortConfig.termios.setStopBits(val);
            }
            else if (aname == "rts485") {
                if (aval == "true" || aval == "1") {
                    _workingPortConfig.rts485 = 1;
                }
                else if (aval == "false" || aval == "0") {
                    _workingPortConfig.rts485 = 0;
                }
                else if (aval == "-1") {
                    _workingPortConfig.rts485 = -1;
                }
                else {
                    throw n_u::InvalidParameterException(
                    string("SerialSensor:") + getName(),
                        aname, aval);
                }
            }
            else if (aname == "nullterm");
            else if (aname == "init_string");
            else if (aname == "suffix");
            else if (aname == "height");
            else if (aname == "depth");
            else if (aname == "duplicateIdOK");
            else if (aname == "timeout");
            else if (aname == "readonly");
            else if (aname == "station");
            else if (aname == "autoconfig");
                else if (aname == "xml:base" || aname == "xmlns") {}
            else throw n_u::InvalidParameterException(
            string("SerialSensor:") + getName(),
            "unknown attribute",aname);

        }
    }

    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0; child=child->getNextSibling()) {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();

        if (elname == "message");
        else if (elname == "prompt");
        else if (elname == "sample");
        else if (elname == "parameter");
        else if (elname == "calfile");
        else if (elname == "autoconfig");
        else throw n_u::InvalidParameterException(
            string("SerialSensor:") + getName(),
            "unknown element",elname);
    }
}

/**
 *  Autoconfig functions
 */

void SerialSensor::doAutoConfig()
{
	// find out if we're a legacy subclass or a new autoconfig subclass
	if (supportsAutoConfig()) {
		// Must be a new autoconfig subclass...
		if (findWorkingSerialPortConfig()) {
			NLOG(("Found working sensor serial port configuration"));
			NLOG((""));
			NLOG(("Attempting to install the desired sensor serial parameter configuration"));
			if (installDesiredSensorConfig(_workingPortConfig)) {
				NLOG(("Desired sensor serial port configuration successfully installed"));
				NLOG((""));
				NLOG(("Attempting to install the desired sensor science configuration"));
				if (configureScienceParameters()) {
					NLOG(("Desired sensor science configuration successfully installed"));
				}
				else {
					NLOG(("Failed to install sensor science configuration"));
				}
			}
			else {
				NLOG(("Failed to install desired config. Reverted back to what works. "
						"Science configuration is not installed."));
			}
		}
		else
		{
			NLOG(("Couldn't find a serial port configuration that worked with this PTB210 sensor. "
				  "May need to troubleshoot the sensor or cable. "
				  "!!!NOTE: Sensor is not open for data collection!!!"));
		}
	}
}

bool SerialSensor::findWorkingSerialPortConfig()
{
    bool foundIt = false;

    // first see if the current configuration is working. If so, all done!
    if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
        NLOG(("Testing initial config which may be custom "));
        printPortConfig();
    }

    if (!doubleCheckResponse()) {
        // initial config didn't work, so sweep through all parameters starting w/the default
        if (!isDefaultConfig(getPortConfig())) {
            // it's a custom config, so test default first
            NLOG(("Testing default config because SerialSensor applied a custom config which failed"));
            if (!testDefaultPortConfig()) {
                NLOG(("Default PortConfig failed. Now testing all the other serial parameter configurations..."));
                foundIt = sweepCommParameters();
            }
            else {
                // found it!! Tell someone!!
                foundIt = true;
                if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                    NLOG(("Default PortConfig was successfull!!!"));
                    printPortConfig();
                }
            }
        }
        else {
            NLOG(("Default PortConfig was not changed and failed. Now testing all the other serial "
                  "parameter configurations..."));
            foundIt = sweepCommParameters();
        }
    }
    else {
        // Found it! Tell someone!
        if (!isDefaultConfig(getPortConfig())) {
            NLOG(("SerialSensor customimized the default PortConfig and it succeeded!!"));
        }
        else {
            NLOG(("SerialSensor did not customimize the default PortConfig and it succeeded!!"));
        }

        foundIt = true;
        if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
            printPortConfig();
        }
    }

    return foundIt;
}

bool SerialSensor::sweepCommParameters()
{
    bool foundIt = false;

    for (PortTypeList::iterator portTypeIter = _portTypeList.begin();
    	 portTypeIter != _portTypeList.end() && !foundIt;
    	 ++portTypeIter) {
        int rts485 = 0;
        PORT_TYPES portType = *portTypeIter;
        DLOG(("Checking port type: ") << SerialPortIODevice::portTypeToStr(portType));

        if (portType == RS485_HALF)
            rts485 = -1; // ??? TODO check this out: start low. Let write manage setting high
        else if (portType == RS422)
            rts485 = -1; // always high, since there are two drivers going both ways

        for (BaudRateList::iterator baudRateIter = _baudRateList.begin();
        	 baudRateIter != _baudRateList.end() && !foundIt;
        	 ++baudRateIter) {
            int baud = *baudRateIter;
            DLOG(("Checking baud rate: ") << baud);

            for (WordSpecList::iterator wordSpecIter = _serialWordSpecList.begin();
            	 wordSpecIter != _serialWordSpecList.end() && !foundIt;
            	 ++wordSpecIter) {
                WordSpec wordSpec = *wordSpecIter;
                DLOG(("Checking serial word spec: ") << (*wordSpecIter).dataBits
                									 << Termios::parityToString((*wordSpecIter).parity, true)
                									 << (*wordSpecIter).stopBits);

                // get the existing port config to preserve the port
                // which only gets set on construction
                PortConfig testPortConfig = getPortConfig();

                // now set it to the new parameters
                setTargetPortConfig(testPortConfig, baud, wordSpec.dataBits, wordSpec.parity,
                                                    wordSpec.stopBits, rts485, portType, NO_TERM,
                                                    _defaultPortConfig.xcvrConfig.sensorPower);

                if (LOG_LEVEL_IS_ACTIVE(LOGGER_DEBUG)) {
                    DLOG(("Asking for PortConfig:"));
                    printTargetConfig(testPortConfig);
                }

                // don't test the default as it's already tested.
                if (isDefaultConfig(testPortConfig))
                {
                    // skip
                    NLOG((""));
                    NLOG(("Skipping default configuration since it's already tested..."));
                    continue;
                }

                setPortConfig(testPortConfig);
                applyPortConfig();

                if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                    NLOG((""));
                    NLOG(("Testing PortConfig: "));
                    printPortConfig();
                }

                if (doubleCheckResponse()) {
                    foundIt = true;
                    break;
                }
                else {
                    if (portType == RS485_HALF || portType == RS422) {
                        DLOG(("If 422/485, one more try - test the connection w/termination turned on."));
                        setTargetPortConfig(testPortConfig, baud, wordSpec.dataBits, wordSpec.parity,
                                                            wordSpec.stopBits, rts485, portType, TERM_120_OHM,
                                                            _defaultPortConfig.xcvrConfig.sensorPower);
                        if (LOG_LEVEL_IS_ACTIVE(LOGGER_DEBUG)) {
                            DLOG(("Asking for PortConfig:"));
                            printTargetConfig(testPortConfig);
                        }

                        setPortConfig(testPortConfig);
                        applyPortConfig();

                        if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                            NLOG(("Testing PortConfig on RS422/RS485 with termination: "));
                            printPortConfig();
                        }

                        if (doubleCheckResponse()) {
                            foundIt = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    return foundIt;
}

void SerialSensor::setTargetPortConfig(PortConfig& target, int baud, int dataBits, Termios::parity parity, int stopBits,
														   int rts485, PORT_TYPES portType, TERM termination,
														   SENSOR_POWER_STATE power)
{
    target.termios.setBaudRate(baud);
    target.termios.setDataBits(dataBits);
    target.termios.setParity(parity);
    target.termios.setStopBits(stopBits);
    target.rts485 = (rts485);
    target.xcvrConfig.portType = portType;
    target.xcvrConfig.termination = termination;
    target.xcvrConfig.sensorPower = power;

    target.applied =false;
}

bool SerialSensor::isDefaultConfig(const PortConfig& rTestConfig) const
{
    return ((rTestConfig.termios.getBaudRate() == _defaultPortConfig.termios.getBaudRate())
            && (rTestConfig.termios.getParity() == _defaultPortConfig.termios.getParity())
            && (rTestConfig.termios.getDataBits() == _defaultPortConfig.termios.getDataBits())
            && (rTestConfig.termios.getStopBits() == _defaultPortConfig.termios.getStopBits())
            && (rTestConfig.rts485 == _defaultPortConfig.rts485)
            && (rTestConfig.xcvrConfig.portType == _defaultPortConfig.xcvrConfig.portType)
            && (rTestConfig.xcvrConfig.termination == _defaultPortConfig.xcvrConfig.termination)
            && (rTestConfig.xcvrConfig.sensorPower == _defaultPortConfig.xcvrConfig.sensorPower));
}

bool SerialSensor::testDefaultPortConfig()
{
	// get the existing PortConfig to preserve the serial device
    PortConfig testPortConfig = getPortConfig();

    // copy in the defaults
    setTargetPortConfig(testPortConfig,
						_defaultPortConfig.termios.getBaudRate(),
						_defaultPortConfig.termios.getDataBits(),
						_defaultPortConfig.termios.getParity(),
						_defaultPortConfig.termios.getStopBits(),
						_defaultPortConfig.rts485,
						_defaultPortConfig.xcvrConfig.portType,
						_defaultPortConfig.xcvrConfig.termination,
						_defaultPortConfig.xcvrConfig.sensorPower);

    // send it back up the hierarchy
    setPortConfig(testPortConfig);

    // apply it to the hardware
    applyPortConfig();

    // test it
    return doubleCheckResponse();
}

bool SerialSensor::doubleCheckResponse()
{
    bool foundIt = false;

    DLOG(("Checking response once..."));
    if (checkResponse()) {
        if (LOG_LEVEL_IS_ACTIVE(LOGGER_DEBUG)) {
            // tell everyone
            DLOG(("Found working port config: "));
            printPortConfig();
        }

        foundIt = true;
    }
    else {
        DLOG(("Checking response twice..."));
        if (checkResponse()) {
            // tell everyone
            DLOG(("Response checks out on second try..."));
            if (LOG_LEVEL_IS_ACTIVE(LOGGER_DEBUG)) {
                DLOG(("Found working port config: "));
                printPortConfig();
            }

            foundIt = true;
        }
        else {
            DLOG(("Checked response twice, and failed twice."));
        }
    }

    return foundIt;
}

bool SerialSensor::configureScienceParameters()
{
    DLOG(("Sending sensor science parameters."));
    sendScienceParameters();
    DLOG(("First check of desired science parameters"));
    bool success = checkScienceParameters();
    if (!success) {
        DLOG(("First attempt to send science parameters failed - resending"));
        sendScienceParameters();
        success = checkScienceParameters();
        if (!success) {
            DLOG(("Second attempt to send science parameters failed. Giving up."));
        }
    }

    return success;
}

SerialSensor::Prompter::~Prompter()
{
    delete [] _prompt;
}

void SerialSensor::Prompter::setPrompt(const string& val)
{
    delete [] _prompt;
    _promptLen = val.length();
    _prompt = new char[_promptLen+1];
    // Use memcpy, not strcpy since the prompt may contain
    // null chars.
    memcpy(_prompt,val.c_str(),_promptLen);
}

void SerialSensor::Prompter::setPromptPeriodMsec(const int val)
{
    _promptPeriodMsec = val;
}

void SerialSensor::Prompter::setPromptOffsetMsec(const int val)
{
    _promptOffsetMsec = val;
}

void SerialSensor::Prompter::looperNotify() throw()
{
    if (!_prompt) return;
    try {
	_sensor->write(_prompt,_promptLen);
    }
    catch(const n_u::IOException& e) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: write prompt: %s",_sensor->getName().c_str(),
	    e.what());
    }
}

