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

#include "Looper.h"
#include "Prompt.h"

#include <nidas/util/PowerCtrlIf.h>
#include <nidas/util/Logger.h>
#include <nidas/util/SPoll.h>

#ifdef HAVE_BLUETOOTH_RFCOMM_H
#include "BluetoothRFCommSocketIODevice.h"
#endif

#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <locale>

using namespace std;
using namespace nidas::core;
using namespace nidas::util;

SerialSensor::SerialSensor():
    _autoConfigSupported(false), _autoConfigEnabled(false), _desiredPortConfig(), _portTypeList(), 
    _baudRateList(), _serialWordSpecList(),_autoConfigState(AUTOCONFIG_UNSUPPORTED),
    _serialState(AUTOCONFIG_UNSUPPORTED),_scienceState(AUTOCONFIG_UNSUPPORTED),
    _deviceState(AUTOCONFIG_UNSUPPORTED),_configMode(NOT_ENTERED), _initPowerState(n_u::POWER_ON),
    _defaultPortConfig(),_serialDevice(0), _prompters(), _prompting(false)
{
    setDefaultMode(O_RDWR|O_NOCTTY|O_NDELAY);
    _desiredPortConfig.termios.setRaw(true);
    _desiredPortConfig.termios.setRawLength(1);
    _desiredPortConfig.termios.setRawTimeout(0);
    _defaultPortConfig.termios.setRaw(true);
    _defaultPortConfig.termios.setRawLength(1);
    _defaultPortConfig.termios.setRawTimeout(0);
}

SerialSensor::SerialSensor(const PortConfig& rInitPortConfig, POWER_STATE initPowerState):
    _autoConfigSupported(false), _autoConfigEnabled(false), _desiredPortConfig(rInitPortConfig), 
    _portTypeList(), _baudRateList(), _serialWordSpecList(),_autoConfigState(AUTOCONFIG_UNSUPPORTED),
    _serialState(AUTOCONFIG_UNSUPPORTED),_scienceState(AUTOCONFIG_UNSUPPORTED),
    _deviceState(AUTOCONFIG_UNSUPPORTED),_configMode(NOT_ENTERED), _initPowerState(initPowerState),
    _defaultPortConfig(rInitPortConfig),_serialDevice(0), _prompters(), _prompting(false)
{
    setDefaultMode(O_RDWR|O_NOCTTY|O_NDELAY);
    _desiredPortConfig.termios.setRaw(true);
    _desiredPortConfig.termios.setRawLength(1);
    _desiredPortConfig.termios.setRawTimeout(0);
    _defaultPortConfig.termios.setRaw(true);
    _defaultPortConfig.termios.setRawLength(1);
    _defaultPortConfig.termios.setRawTimeout(0);
}

SerialSensor::~SerialSensor()
{
    list<Prompter*>::const_iterator pi = _prompters.begin();
    for (; pi != _prompters.end(); ++pi) delete *pi;

    _serialDevice = 0;
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
        _serialDevice = new SerialPortIODevice(getDeviceName(), _desiredPortConfig);
        if (!_serialDevice) {
        	throw Exception("SerialSensor::buildIODevice(): failed to instantiate SerialPortIODevice object.");
        }
        device = _serialDevice;

        // update desiredPortConfig with the port ID data which is populated in the SerialPortIODevice ctor
        // this is needed for future comparisons.
        _desiredPortConfig = getPortConfig();

        /*
         *  Create sensor power control object here...
         */

        GPIO_PORT_DEFS portID = _desiredPortConfig.xcvrConfig.port;
        DLOG(("SerialSensor::buildIODevice() : Instantiating SensorPowerCtrl object: ") << n_u::gpio2Str(portID));
        SensorPowerCtrl* pSensorPwrCtrl = new SensorPowerCtrl(portID);
        if (pSensorPwrCtrl == 0)
        {
            throw n_u::Exception("SerialPortIODevice: Cannot construct SensorPowerCtrl object");
        }
        setPowerCtrl(pSensorPwrCtrl);
        enablePwrCtrl(true);
        setPower(_initPowerState);
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

void SerialSensor::sendInitString() throw(nidas::util::IOException)
{
    NLOG(("%s:%s: Putting sensor into measurement mode", getName().c_str(), getClassName().c_str()));
    if (getAutoConfigEnabled()) {
        exitConfigMode();
    }
    // XXXXX This looks suspicious.  Why can't an "autoconfig" sensor also send
    // an init string before sampling?
    else {
        if (getInitString().length() != 0) {
            CharacterSensor::sendInitString();
        }
    }
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
             DLOG(("Flushed serial port input on device: ") << getName());
        }
        else if (accmode == O_WRONLY) {
            _serialDevice->flushOutput();
             DLOG(("Flushed serial port output on device: ") << getName());
        }
        else {
            _serialDevice->flushBoth();
             DLOG(("Flushed serial port input and output on device: ") << getName());
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
    static LogContext lp(LOG_DEBUG);
    if (lp.active()) {
        lp.log() << "SerialSensor::applyPortConfig(): Initial power state.";
        printPowerState();
    }

    if (_serialDevice) {
        _serialDevice->applyPortConfig();
    }
    else {
        NLOG(("SerialSensor::applyPortConfig(): device, ") << getName() 
             << (", is trying to apply PortConfig too early, "
                 "or is newer type serial device such as USB or socket-oriented, "
                 " which has no need of a PortConfig."));
    }

    if (lp.active()) {
        lp.log() << "SerialSensor::applyPortConfig(): Current power state.";
        printPowerState();
    }
}

void SerialSensor::printPortConfig(bool flush)
{
    if (_serialDevice) {
        _serialDevice->printPortConfig();
        printPowerState();
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
    for (auto& pi : getPrompts())
    {
        if (! pi.valid())
            continue;
        Prompt prompt = pi;
        Prompter* prompter = new Prompter(this);
        prompt.setString(n_u::replaceBackslashSequences(prompt.getString()));
        prompter->setPrompt(prompt);
        int periodms = (int) rint(MSECS_PER_SEC / prompt.getRate());
        int offsetms = (int) rint(MSECS_PER_SEC * prompt.getOffset());
        prompter->setPromptPeriodMsec(periodms);
        prompter->setPromptOffsetMsec(offsetms);
        _prompters.push_back(prompter);
        DLOG(("") << "sensor " << getName() << " prompter: "
                    << prompt << " @ period=" << periodms << "ms, "
                    << "offset=" << offsetms << "ms");
    }
    startPrompting();
}

void SerialSensor::shutdownPrompting() throw(n_u::IOException)
{
    stopPrompting();
    for (auto prompter : _prompters)
    {
        delete prompter;
    }
    _prompters.clear();
}

void SerialSensor::startPrompting() throw(n_u::IOException)
{
    for (auto prompter : _prompters) {
        getLooper()->addClient(prompter,
                prompter->getPromptPeriodMsec(),
                prompter->getPromptOffsetMsec());
        _prompting = true;
    }
}

void SerialSensor::stopPrompting() throw(n_u::IOException)
{
    for (auto prompter : _prompters) {
        getLooper()->removeClient(prompter);
    }
    _prompting = false;
}

void SerialSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    if (_serialDevice) {

        try {
			ostr << "<td align=left>" << autoCfgToStr(_autoConfigState);
			if (_autoConfigState == AUTOCONFIG_STARTED) {
				ostr << "\n" << autoCfgToStr(_serialState);
				if (_serialState != CONFIGURING_COMM_PARAMETERS) {
					ostr << "\n" << autoCfgToStr(_scienceState);
				}
			}
			ostr << "\n" << getPortConfig().termios.getBaudRate() <<
				getPortConfig().termios.getParityString().substr(0,1) <<
				getPortConfig().termios.getDataBits() << getPortConfig().termios.getStopBits();
			if (getReadFd() < 0) {
				ostr << ",<font color=red><b>not active</b></font>";
				if (getTimeoutMsecs() > 0) {
					ostr << ",timeouts=" << getTimeoutCount();
				}
				ostr << "</td>" << endl;
				return;
			}
			if (getTimeoutMsecs() > 0) {
				ostr << ",timeouts=" << getTimeoutCount();
			}
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

/*
 *  Handles common port config attributes in autoconfig tag.
 */
void SerialSensor::fromDOMElementAutoConfig(const xercesc::DOMElement* node)
{
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child = child->getNextSibling()) {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
            continue;

        XDOMElement xchild((xercesc::DOMElement*) (child));
        const string& elname = xchild.getNodeName();
        if (elname == "autoconfig") {
            DLOG(("SerialSensor::fromDOMElementAutoconfig(): autoconfig tag found."));
            if (supportsAutoConfig()) {
                setAutoConfigEnabled();

                // get all the attributes of the node
                xercesc::DOMNamedNodeMap* pAttributes = child->getAttributes();
                int nSize = pAttributes->getLength();
                for (int i = 0; i < nSize; ++i) {
                    XDOMAttr attr((xercesc::DOMAttr*) (pAttributes->item(i)));
                    std::string aname = attr.getName();
                    if (aname == "porttype" || aname == "termination") {
                        checkXcvrConfigAttribute(attr);
                    }
                    else if (aname == "baud" || aname == "parity" 
                             || aname == "databits" || aname == "stopbits") {
                        checkTermiosConfigAttribute(attr);
                    }
                }
            }
        }
        else if (elname == "message");
        else if (elname == "prompt");
        else if (elname == "sample");
        else if (elname == "parameter");
        else if (elname == "calfile");
        else
            throw n_u::InvalidParameterException(
                    getName(), "unknown element", elname);
    }
}

/*
 *  Handles common FTDI Autoconfig supported port config attributes.
 */
void SerialSensor::checkXcvrConfigAttribute(const XDOMAttr& rAttr)
{
    // get attribute name
    const std::string& aname = rAttr.getName();
    const std::string& aval = rAttr.getValue();
    // xform everything to uppercase - this shouldn't affect numbers
    string upperAval = aval;
    std::transform(upperAval.begin(), upperAval.end(),
            upperAval.begin(), ::toupper);
    DLOG(("SerialSensor:checkXcvrConfigAttribute(): attribute: ") << aname << " : " << upperAval);
    if (aname == "porttype") {
        if (upperAval == "RS232")
            _desiredPortConfig.xcvrConfig.portType = RS232;
        else if (upperAval == "RS422")
            _desiredPortConfig.xcvrConfig.portType = RS422;
        else if (upperAval == "RS485_HALF")
            _desiredPortConfig.xcvrConfig.portType = RS485_HALF;
        else if (upperAval == "RS485_FULL")
            _desiredPortConfig.xcvrConfig.portType = RS485_FULL;
        else
            throw n_u::InvalidParameterException(
                    getName(), aname, aval);
    }
    else if (aname == "termination") {
        if (upperAval == "NO_TERM" || upperAval == "NO"
                || upperAval == "FALSE")
            _desiredPortConfig.xcvrConfig.termination = NO_TERM;
        else if (upperAval == "TERM_120_OHM" || upperAval == "YES"
                || upperAval == "TRUE")
            _desiredPortConfig.xcvrConfig.termination =
                    TERM_120_OHM;
        else
            throw n_u::InvalidParameterException(
                    getName(), aname, aval);
    }
    else if (aname == "rts485") {
        if (upperAval == "TRUE" || aval == "1") {
            _desiredPortConfig.rts485 = 1;
        }
        else if (upperAval == "FALSE" || aval == "0") {
            _desiredPortConfig.rts485 = 0;
        }
        else if (aval == "-1") {
            _desiredPortConfig.rts485 = -1;
        }
    }
}

/*
 *  Handles common termios config attributes.
 */
void SerialSensor::checkTermiosConfigAttribute(const XDOMAttr& rAttr)
{
    // get attribute name
    const std::string& aname = rAttr.getName();
    const std::string& aval = rAttr.getValue();
    // xform everything to uppercase - this shouldn't affect numbers
    string upperAval = aval;
    std::transform(upperAval.begin(), upperAval.end(),
            upperAval.begin(), ::toupper);
    DLOG(("SerialSensor:checkTermiosConfigAttribute(): attribute: ") << aname << " : " << upperAval);
    if (aname == "baud") {
        istringstream ist(aval);
        int val;
        ist >> val;
        if (ist.fail()
            || !_desiredPortConfig.termios.setBaudRate(val)) {
            throw n_u::InvalidParameterException(
                getName(), aname, aval);
        }
    }
    else if (aname == "parity") {
        if (upperAval == "ODD") {
            _desiredPortConfig.termios.setParity(n_u::Termios::ODD);
        }
        else if (upperAval == "EVEN") {
            _desiredPortConfig.termios.setParity(
                    n_u::Termios::EVEN);
        }
        else if (upperAval == "NONE") {
            _desiredPortConfig.termios.setParity(
                    n_u::Termios::NONE);
        }
        else {
            throw n_u::InvalidParameterException(
                    getName(), aname, aval);
        }
    }
    else if (aname == "databits") {
        istringstream ist(aval);
        int val;
        ist >> val;
        if (ist.fail() || val < 5 || val > 8) {
            throw n_u::InvalidParameterException(
                    getName(), aname, aval);
        }

        _desiredPortConfig.termios.setDataBits(val);
    }
    else if (aname == "stopbits") {
        istringstream ist(aval);
        int val;
        ist >> val;
        if (ist.fail() || val < 1 || val > 2) {
            throw n_u::InvalidParameterException(
                    getName(), aname, aval);
        }

        _desiredPortConfig.termios.setStopBits(val);
    }
}

void SerialSensor::fromDOMElement(
	const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    DLOG(("SerialSensor::fromDOMElement(): entry..."));

    CharacterSensor::fromDOMElement(node);

    DLOG(("SuperClass::fromDOMElement() methods done..."));

    if(node->hasAttributes()) {
        // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();

            if (aname == "ID")/*ignore*/;
            else if (aname == "IDREF");
            else if (aname == "class");
            else if (aname == "devicename");
            else if (aname == "id");
            else if (aname == "baud" || aname == "databits" 
                     || aname == "parity" || aname == "stopbits") {
                checkTermiosConfigAttribute(attr);
            }
            else if (aname == "porttype" || aname == "termination"
                     || aname == "rts485") {
                // We always check these, because we don't have an IoDevice
                // built yet, which checks whether there is an FTDI board 
                // capable of setting the port xcvr mode.
                checkXcvrConfigAttribute(attr);
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
            else if (aname == "xml:base" || aname == "xmlns") {}
            else throw n_u::InvalidParameterException(
                    getName(), "unknown attribute",aname);
        }
    }

    DLOG(("SerialSensor::fromDOMElement(): exit..."));
}

/**
 *  Autoconfig functions
 */

void SerialSensor::initAutoConfig()
{
    if (supportsAutoConfig()) {
        _autoConfigState = WAITING_IDLE;
        _serialState = WAITING_IDLE;
        _scienceState = WAITING_IDLE;
        _deviceState = WAITING_IDLE;
    }
}

void SerialSensor::doAutoConfig()
{
	// find out if we're a legacy subclass or a new autoconfig subclass
	if (getAutoConfigEnabled()) {
	    setSensorState(SENSOR_CONFIGURING);
		_autoConfigState = AUTOCONFIG_STARTED;
		// Must be a new autoconfig subclass...
		_serialState = CONFIGURING_COMM_PARAMETERS;
		if (findWorkingSerialPortConfig()) {
			NLOG(("Found working sensor serial port configuration"));
			NLOG((""));
			NLOG(("Checking whether found working port config is the desired port config"));
			PortConfig foundWorkingConfig = getPortConfig();
			if (foundWorkingConfig != _desiredPortConfig) {
				NLOG(("found working config not equal to desired config"));
				NLOG(("found working config: ") << foundWorkingConfig);
				NLOG(("desired port config: ") << _desiredPortConfig);
				NLOG(("Attempting to install the desired sensor serial parameter configuration"));
				if (installDesiredSensorConfig(_desiredPortConfig)) {
					NLOG(("Desired sensor serial port configuration successfully installed"));
					_serialState = COMM_PARAMETER_CFG_SUCCESSFUL;
				}
				else {
					NLOG(("Failed to install desired config. Reverting back to what works. "));
					installDesiredSensorConfig(foundWorkingConfig);
					_serialState = COMM_PARAMETER_CFG_UNSUCCESSFUL;
					_autoConfigState = AUTOCONFIG_UNSUCCESSFUL;
		            setSensorState(SENSOR_CONFIGURE_FAILED);
				}
			}
			else {
				NLOG(("Found working port config is same as desired port config."));
				_serialState = COMM_PARAMETER_CFG_SUCCESSFUL;
			}

            NLOG(("Collecting sensor metadata..."));
            updateMetaData();
            NLOG(("Attempting to configure the sensor science parameters."));
            _scienceState = CONFIGURING_SCIENCE_PARAMETERS;
            if (configureScienceParameters()) {
                NLOG(("Desired sensor science configuration successfully installed"));
                _scienceState = SCIENCE_SETTINGS_SUCCESSFUL;
                _autoConfigState =
                        _serialState == COMM_PARAMETER_CFG_SUCCESSFUL ?
                                        AUTOCONFIG_SUCCESSFUL : AUTOCONFIG_UNSUCCESSFUL;
                setSensorState(_autoConfigState == AUTOCONFIG_SUCCESSFUL ?
                               SENSOR_CONFIGURE_SUCCEEDED : SENSOR_CONFIGURE_FAILED);
            }
            else {
                NLOG(("Failed to install sensor science configuration"));
                _scienceState = SCIENCE_SETTINGS_UNSUCCESSFUL;
                _autoConfigState = AUTOCONFIG_UNSUCCESSFUL;
                setSensorState(SENSOR_CONFIGURE_FAILED);
            }
		}
		else
		{
			NLOG(("Couldn't find a serial port configuration that worked with this sensor. "
				  "May need to troubleshoot the sensor or cable. "
				  "!!!NOTE: Sensor is NOT ready for data collection!!!"));
			_serialState = COMM_PARAMETER_CFG_UNSUCCESSFUL;
			_autoConfigState = AUTOCONFIG_UNSUCCESSFUL;
			setSensorState(SENSOR_CONFIGURE_FAILED);
		}

		printDeviceMetaData();
	}
	else {
	    NLOG(("Autoconfig is not enabled or is not supported."));
	    NLOG(("But we must still set the termios, xcvr config, turn it on, etc."));
        DLOG(("If the board is an older FTDI board which doesn't support "));
        DLOG(("AutoConfig xcvr control, then only termios is set."));
	    DLOG(("SerialSensor constructor and fromDomElement() should have "));
	    DLOG(("already specified the details."));
        applyPortConfig();
	}
}

bool SerialSensor::findWorkingSerialPortConfig()
{
    bool foundIt = false;

    // first see if the current configuration is working. If so, all done!
    NLOG(("SerialSensor::findWorkingSerialPortConfig(): Testing initial config which may be custom ") << _desiredPortConfig);

    NLOG(("SerialSensor::findWorkingSerialPortConfig(): Entering sensor config mode()"));
    CFG_MODE_STATUS cfgMode = enterConfigMode();

    if (cfgMode == NOT_ENTERED || cfgMode == ENTERED) {
        if (cfgMode == ENTERED) {
            foundIt = doubleCheckResponse();
        }

        if (!foundIt) {
            // initial config didn't work, so sweep through all parameters starting w/the default
            if (!isDefaultConfig(getPortConfig())) {
                // it's a custom config, so test default first
                NLOG(("SerialSensor::findWorkingSerialPortConfig(): Testing default config because SerialSensor applied a custom config which failed"));
                if (!testDefaultPortConfig()) {
                    NLOG(("SerialSensor::findWorkingSerialPortConfig(): Default PortConfig failed. Now testing all the other serial parameter configurations..."));
                    foundIt = sweepCommParameters();
                }
                else {
                    // found it!! Tell someone!!
                    foundIt = true;
                    NLOG(("SerialSensor::findWorkingSerialPortConfig(): Default PortConfig was successfull!!!") << getPortConfig());
                }
            }
            else {
                NLOG(("SerialSensor::findWorkingSerialPortConfig(): Default PortConfig was not changed and failed. Now testing all the other serial "
                      "parameter configurations..."));
                foundIt = sweepCommParameters();
            }
        }
        else {
            // Found it! Tell someone!
            if (!isDefaultConfig(getPortConfig())) {
                NLOG(("SerialSensor::findWorkingSerialPortConfig(): SerialSensor customized the default PortConfig and it succeeded!!"));
            }
            else {
                NLOG(("SerialSensor::findWorkingSerialPortConfig(): SerialSensor did not customize the default PortConfig and it succeeded!!"));
            }

            foundIt = true;
            NLOG(("") << getPortConfig());
        }
    }
    else if (cfgMode == ENTERED_RESP_CHECKED) {
        // Found it! Tell someone!
        if (!isDefaultConfig(getPortConfig())) {
            NLOG(("SerialSensor::findWorkingSerialPortConfig(): SerialSensor customized the default PortConfig and it succeeded!!"));
        }
        else {
            NLOG(("SerialSensor::findWorkingSerialPortConfig(): SerialSensor did not customize the default PortConfig and it succeeded!!"));
        }

        foundIt = true;
        NLOG(("") << getPortConfig());
    }

    else {
        NLOG(("SerialSensor::findWorkingSerialPortConfig(): %s:%s: SerialSensor: subclass returned an unknown response from enterConfigMode()",
              getName().c_str(),getClassName().c_str()));
        throw n_u::IOException(getName()+":"+getClassName(),
                               "SerialSensor::findWorkingPortConfig(): unknown response returned by enterConfigMode(): ",
                               cfgMode);
    }
    return foundIt;
}

bool SerialSensor::sweepCommParameters()
{
    bool foundIt = false;
    CFG_MODE_STATUS cfgMode = NOT_ENTERED;

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
                DLOG(("Checking serial word spec: ") << wordSpec.dataBits
                									 << Termios::parityToString(wordSpec.parity, true)
                									 << wordSpec.stopBits);

                // get the existing port config to preserve the port
                // which only gets set on construction
                PortConfig testPortConfig = getPortConfig();

                // now set it to the new parameters
                setTargetPortConfig(testPortConfig, baud, wordSpec.dataBits, wordSpec.parity,
                                                    wordSpec.stopBits, rts485, portType, NO_TERM);

				DLOG(("Asking for PortConfig:") << testPortConfig);

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

				NLOG((""));
				NLOG(("Testing PortConfig: ") << getPortConfig());
				NLOG(("Power State: ") << getPowerStateStr());

				cfgMode = enterConfigMode();
				if (cfgMode == ENTERED) {
                    if (doubleCheckResponse()) {
                        foundIt = true;
                        break;
                    }

                    else if (portType == RS485_HALF || portType == RS422) {
                        DLOG(("If 422/485, one more try - test the connection w/termination turned on."));
                        setTargetPortConfig(testPortConfig, baud, wordSpec.dataBits, wordSpec.parity,
                                                            wordSpec.stopBits, rts485, portType, TERM_120_OHM);
                        DLOG(("Asking for PortConfig:") << testPortConfig);

                        setPortConfig(testPortConfig);
                        applyPortConfig();

                        NLOG(("Testing PortConfig on RS422/RS485 with termination: ") << getPortConfig());

                        cfgMode = enterConfigMode();
                        if (cfgMode == ENTERED) {
                            if (doubleCheckResponse()) {
                                foundIt = true;
                                break;
                            }
                        }
                        else if (cfgMode == ENTERED_RESP_CHECKED) {
                            foundIt = true;
                            break;
                        }
                        else if (cfgMode != NOT_ENTERED){
                            NLOG(("%s:%s: SerialSensor: subclass returned an unknown response from enterConfigMode()",
                                  getName().c_str(),getClassName().c_str()));
                            throw n_u::IOException(getName()+":"+getClassName(),
                                                   "SerialSensor::sweepCommParameters(): unknown response returned by enterConfigMode(): ",
                                                   cfgMode);
                        }
                    }
                }
                else if (cfgMode == ENTERED_RESP_CHECKED) {
                    foundIt = true;
                    break;
                }
                else if (cfgMode != NOT_ENTERED){
                    NLOG(("%s:%s: SerialSensor: subclass returned an unknown response from enterConfigMode()",
                          getName().c_str(),getClassName().c_str()));
                    throw n_u::IOException(getName()+":"+getClassName(),
                                           "SerialSensor::sweepCommParameters(): unknown response returned by enterConfigMode(): ",
                                           cfgMode);
                }
            }
        }

        /*
         *  If this is running on a DSM which doesn't support serial port transceiver control,
         *  then just break out after first round of serial port parameter checks
         */
        if (!foundIt && !_serialDevice->getXcvrCtrl()) {
            DLOG(("SerialSensor::sweepCommParameters(): "
                  "SerialXcvrCtrl not instantiated, so done "
                  "after first round of serial parameter checks."));
            NLOG(("Couldn't find working serial port parameters. Try changing the device type or transceiver jumpers."));
            break;
        }
    }

    return foundIt;
}

void SerialSensor::setTargetPortConfig(PortConfig& target, int baud, int dataBits, Termios::parity parity, int stopBits,
														   int rts485, PORT_TYPES portType, TERM termination)
{
    target.termios.setBaudRate(baud);
    target.termios.setDataBits(dataBits);
    target.termios.setParity(parity);
    target.termios.setStopBits(stopBits);
    target.rts485 = (rts485);
    target.xcvrConfig.portType = portType;
    target.xcvrConfig.termination = termination;

    target.applied =false;
}

bool SerialSensor::isDefaultConfig(const PortConfig& rTestConfig) const
{
    DLOG(("SerialSensor::isDefaultConfig(): default is - ") << _defaultPortConfig);
    VLOG(("rTestConfig.termios.getBaudRate() == _defaultPortConfig.termios.getBaudRate(): ")
          << (rTestConfig.termios.getBaudRate() == _defaultPortConfig.termios.getBaudRate() ? "true" : "false"));
    VLOG(("rTestConfig.termios.getParity() == _defaultPortConfig.termios.getParity(): ")
          << (rTestConfig.termios.getParity() == _defaultPortConfig.termios.getParity() ? "true" : "false"));
    VLOG(("rTestConfig.termios.getDataBits() == _defaultPortConfig.termios.getDataBits(): ")
          << (rTestConfig.termios.getDataBits() == _defaultPortConfig.termios.getDataBits() ? "true" : "false"));
    VLOG(("rTestConfig.termios.getStopBits() == _defaultPortConfig.termios.getStopBits(): ")
          << (rTestConfig.termios.getStopBits() == _defaultPortConfig.termios.getStopBits() ? "true" : "false"));
    VLOG(("rTestConfig.rts485 == _defaultPortConfig.rts485: ")
          << (rTestConfig.rts485 == _defaultPortConfig.rts485 ? "true" : "false"));
    VLOG(("rTestConfig.xcvrConfig.portType == _defaultPortConfig.xcvrConfig.portType: ")
          << (rTestConfig.xcvrConfig.portType == _defaultPortConfig.xcvrConfig.portType ? "true" : "false"));
    VLOG(("rTestConfig.xcvrConfig.termination == _defaultPortConfig.xcvrConfig.termination: ")
          << (rTestConfig.xcvrConfig.termination == _defaultPortConfig.xcvrConfig.termination ? "true" : "false"));
    return ((rTestConfig.termios.getBaudRate() == _defaultPortConfig.termios.getBaudRate())
            && (rTestConfig.termios.getParity() == _defaultPortConfig.termios.getParity())
            && (rTestConfig.termios.getDataBits() == _defaultPortConfig.termios.getDataBits())
            && (rTestConfig.termios.getStopBits() == _defaultPortConfig.termios.getStopBits())
            && (rTestConfig.rts485 == _defaultPortConfig.rts485)
            && (rTestConfig.xcvrConfig.portType == _defaultPortConfig.xcvrConfig.portType)
            && (rTestConfig.xcvrConfig.termination == _defaultPortConfig.xcvrConfig.termination));
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
						_defaultPortConfig.xcvrConfig.termination);


    // send it back up the hierarchy
    setPortConfig(testPortConfig);

    // apply it to the hardware
    applyPortConfig();

    // print it out
    DLOG(("Testing default port config: ") << testPortConfig);

    if (enterConfigMode()) {
        // test it
        return doubleCheckResponse();
    }
    else {
        DLOG(("SerialSensor::testDefaultPortConfig(): Failed to get into config mode..."));
    }
    return false;
}

int SerialSensor::readResponse(void *buf, int len, int msecTimeout, bool checkPrintable,
                               bool backOffTimeout, int retryTimeoutFactor)
{
    SPoll poller(msecTimeout);
    poller.addPollee(getReadFd(), 0);

    VLOG(("SerialSensor::readResponse(): timeout msec: %i", msecTimeout));

    int res = 0;

    if (msecTimeout >= MSECS_PER_SEC) {
        VLOG(("SerialSensor::readResponse(): Waiting on select just once as timeout >= 1s"));
        try {
            res = poller.poll();
        } catch (IOTimeoutException& e) {
            DLOG(("Select timeout error on: ") << getDeviceName() << e.what());
            return res;
        } catch (IOException& e) {
            DLOG(("General select error on: ") << getDeviceName() << e.what());
            return (std::size_t)0x40000000;
        }
    }
    else {
        // allow for some slop in data coming in...
        VLOG(("SerialSensor::readResponse(): Waiting on select a few times to allow for slowdowns"));
        int tmpto = msecTimeout;
        for (int i=0; res <= 0 && i < 4; ++i) {
            VLOG(("SerialSensor::readResponse(): select loop: i: %i; msec timeout: %i", i, tmpto));
            try {
                res = poller.poll();
            } catch (IOTimeoutException& e) {
                // DLOG(("Select timeout error on: ") << getDeviceName() << e.what());
                // return res;
            } catch (IOException& e) {
                // DLOG(("General select error on: ") << getDeviceName() << e.what());
                // return res;
            }
            VLOG(("SerialSensor::readResponse(): select loop: res: %i; ", res));

            if (res <= 0 && backOffTimeout) {
                VLOG(("SerialSensor::readResponse(): select loop backing off: i: %i; ", i));
                tmpto *= retryTimeoutFactor;
                poller.changeTimeout(tmpto);
            }
        }

        if (res < 0) {
            return (std::size_t)0x40000000;
        }

        if (res == 0) {
            DLOG(("Select timeout on: ") << getDeviceName() << ": " << msecTimeout << " msec");
            return 0;
        }
    }

    VLOG(("SerialSensor::readResponse(): Select successful, reading..."));
    // no select timeout or error, so call read() syscall directly.
    // we also know that there is only one file descriptor, so we don't need 
    // to invoke getNextPolleeEvents()
    std::size_t numChars = ::read(getReadFd(), (char*)buf, (unsigned long)len);
    if (numChars) {
        if (checkPrintable && containsNonPrintable((const char*)buf, numChars)) {
            // not so big assumption here that numChars never gets to 0x80000000
            numChars |= 0x80000000;
        }
    }
    else {
        Exception e(std::string(""), errno);
        DLOG(("SerialSensor::readResponse(): read error: %s", e.what()));
    }

    return numChars;
}

int SerialSensor::readEntireResponse(void *buf, int len, int msecTimeout,
                                     bool checkPrintable, int retryTimeoutFactor)
{
	char* cbuf = (char*)buf;
	int bufRemaining = len;
    int charsReadStatus = readResponse(cbuf, len, msecTimeout, checkPrintable, true, retryTimeoutFactor);

    bool readJunk = charsReadStatus & 0x80000000;
    DLOG(("SerialSensor::readEntireResponse(): readJunk: %s", (readJunk ? "true" : "false")));
    int numCharsRead = charsReadStatus & ~0xC0000000;
    DLOG(("SerialSensor::readEntireResponse(): numCharsRead: %i", numCharsRead));
    int totalCharsRead = numCharsRead;
    bool selectError = charsReadStatus & 0x40000000;
    if (selectError) {
        return 0;
    }

    int bufIdx = totalCharsRead;
    bufRemaining -= numCharsRead;

    static LogContext logctxt(LOG_VERBOSE);
    if (numCharsRead > 0 && logctxt.active()) {
        logctxt.log(nidas::util::LogMessage().format("Initial num chars read is: ")
                                                     << numCharsRead << " comprised of: ");
        printResponseHex(numCharsRead, cbuf);
    }

    int i=0;
    for (; (!readJunk && numCharsRead && bufRemaining > 0); ++i) {
        charsReadStatus = readResponse(&cbuf[bufIdx], bufRemaining, std::max(1, 5*getUsecsPerByte()/USECS_PER_MSEC),
                                       checkPrintable, true, retryTimeoutFactor);
        readJunk = charsReadStatus & 0x80000000;
        numCharsRead = charsReadStatus & ~0x80000000;
        if (readJunk) {
            DLOG(("SerialSensor::readEntireResponse(): Got junk. Bailing out of read loop."));
            totalCharsRead = -1;
            break;
        }
        totalCharsRead += numCharsRead;
        bufIdx = totalCharsRead;
        if (totalCharsRead >= len) {
            bufIdx = len;
            bufRemaining = 0;
            break;
        }
        bufRemaining -= numCharsRead;
        VLOG(("SerialSensor::readEntireResponse(): looping: %i, total chars: %i", i, totalCharsRead));
        VLOG(("SerialSensor::readEntireResponse(): num chars: %i, buf remaining: %i", numCharsRead, bufRemaining));
    }

    if (bufIdx > 0 && logctxt.active()) {
        logctxt.log(nidas::util::LogMessage().format("Num chars read: ")
                                                     << bufIdx << " comprised of: ");
        printResponseHex(totalCharsRead, cbuf);
    }

    // getting garbage, bail out early, but first drain the swamp
    if (readJunk) {
        DLOG(("SerialSensor::readEntireResponse(): Got junk. Draining the swamp."));
        bufRemaining -= numCharsRead;
        bufIdx += numCharsRead;
        int drainedChars = 0;
        do {
            numCharsRead = readResponse(&cbuf[bufIdx], bufRemaining,
                                        std::max(1, 2*getUsecsPerByte()/USECS_PER_MSEC), false);
            drainedChars += numCharsRead;
            bufIdx += numCharsRead;
            bufRemaining -= numCharsRead;
            if (bufRemaining < 0) {
                bufRemaining = 0;
            }
        } while (numCharsRead && bufRemaining > 0);
        VLOG(("SerialSensor::readEntireResponse(): got garbage, so drained ")
             << drainedChars << " characters.");
    }
    VLOG(("Took ") << i+1 << " reads to get entire response");

    return totalCharsRead;
}

bool SerialSensor::doubleCheckResponse()
{
    bool foundIt = false;

    DLOG(("Checking response once..."));
    if (checkResponse()) {
		// tell everyone
		DLOG(("Found working port config: ") << getPortConfig());

        foundIt = true;
    }
    else {
        DLOG(("Checking response twice..."));
        if (checkResponse()) {
            // tell everyone
            DLOG(("Response checks out on second try..."));
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

std::string SerialSensor::autoCfgToStr(AUTOCONFIG_STATE autoState)
{
	std::string stateStr;

	switch (autoState) {
		case AUTOCONFIG_UNSUPPORTED:
			stateStr = "Autoconfig unsupported";
			break;
		case WAITING_IDLE:
			stateStr = "Waiting/idle";
			break;
		case AUTOCONFIG_STARTED:
			stateStr = "Autoconfig started";
			break;
		case CONFIGURING_COMM_PARAMETERS:
			stateStr = "Configuring comm params";
			break;
		case COMM_PARAMETER_CFG_SUCCESSFUL:
			stateStr = "Comm cfg successful";
			break;
		case COMM_PARAMETER_CFG_UNSUCCESSFUL:
			stateStr = "Comm cfg unsuccessful";
			break;
		case CONFIGURING_SCIENCE_PARAMETERS:
			stateStr = "Configuring science params";
			break;
		case SCIENCE_SETTINGS_SUCCESSFUL:
			stateStr = "Science cfg successfull";
			break;
		case SCIENCE_SETTINGS_UNSUCCESSFUL:
			stateStr = "Science cfg unsuccessful";
			break;
		case AUTOCONFIG_SUCCESSFUL:
			stateStr = "Autoconfig successful";
			break;
		case AUTOCONFIG_UNSUCCESSFUL:
			stateStr = "Autoconfig unsuccessful";
			break;
		default:
			break;
	}

	return stateStr;
}

void SerialSensor::printResponseHex(int numCharsRead, const char* respBuf)
{
    if (numCharsRead > 0) {
        for (int i = 0; i < 5; ++i) {
            if ((i*10) > numCharsRead)
                break;

            char hexBuf[70];
            memset(hexBuf, 0, 70);
            for (int j = 0; j < 10; ++j) {
                if ((i*10 + j) > numCharsRead)
                    break;
                snprintf(&(hexBuf[j * 5]), 6, "0x%02x ", respBuf[(i * 10) + j]);
            }

            char* pBytes = &hexBuf[0];
            std::cout << std::string(pBytes) << std::endl;
        }

        DLOG(("SerialSensor::printResponseHex(): all done..."));
	}
}


SerialSensor::Prompter::~Prompter()
{
}

void SerialSensor::Prompter::setPrompt(const Prompt& prompt)
{
    _prompt = prompt;
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
    // Set the response prefix before writing the prompt, to avoid a race
    // condition with the response.  Presumably the prompt has been timed so
    // that setting the prefix now will not interfere with any messages coming
    // from the sensor.
    if (_prompt.hasPrefix())
    {
        _sensor->setPrefix(_prompt.getPrefix());
    }
    const std::string& msg = _prompt.getString();
    if (msg.size())
    {
        try {
            _sensor->write(msg.data(), msg.size());
        }
        catch(const n_u::IOException& e) {
            n_u::Logger::getInstance()->log(LOG_ERR,
                "%s: write prompt: %s",_sensor->getName().c_str(),
                e.what());
        }
    }
}
