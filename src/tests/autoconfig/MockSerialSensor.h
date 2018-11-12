
#ifndef TEST_SERIAL_SENSOR_HPP
#define MOCK_SERIAL_SENSOR_HPP

#include "SerialSensor.h"
#include "../../nidas/util/SerialPort.h"

#include "MockSerialPortIODevice.h"

using namespace nidas::core;

extern PortConfig deviceOperatingPortConfig;

// Expose some protected methods in SerialSensor...
class MockSerialSensor : public SerialSensor
{

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    MockSerialSensor()
    : SerialSensor(), defaultMessageConfig(0, "\r", true)
    {
        // We set the defaults at construction,
        // letting the base class modify according to fromDOMElement()
        setMessageParameters(defaultMessageConfig);

        _portTypeList.push_back(RS232);
        _portTypeList.push_back(RS422);
        _portTypeList.push_back(RS485_HALF);

        _baudRateList.push_back(4800);
        _baudRateList.push_back(9600);
        _baudRateList.push_back(19200);
        _baudRateList.push_back(38400);

        _serialWordSpecList.push_back(WordSpec(7, Termios::EVEN, 1));
        _serialWordSpecList.push_back(WordSpec(7, Termios::ODD, 1));
        _serialWordSpecList.push_back(WordSpec(8, Termios::NONE, 1));

        setDeviceName("/tmp/ttyUSB0");
        SerialPortIODevice* pIODevice = new MockSerialPortIODevice(getDeviceName(), _desiredPortConfig);
        setIODevice(pIODevice);
        setSerialPortIODevice(pIODevice);
    }

    MockSerialSensor(const PortConfig& rInitPortConfig)
    : SerialSensor(rInitPortConfig), defaultMessageConfig(0, "\r", true)
    {
        // We set the defaults at construction,
        // letting the base class modify according to fromDOMElement()
        setMessageParameters(defaultMessageConfig);

        _portTypeList.push_back(RS232);
        _portTypeList.push_back(RS422);
        _portTypeList.push_back(RS485_HALF);

        _baudRateList.push_back(4800);
        _baudRateList.push_back(9600);
        _baudRateList.push_back(19200);
        _baudRateList.push_back(38400);

        _serialWordSpecList.push_back(WordSpec(7, Termios::EVEN, 1));
        _serialWordSpecList.push_back(WordSpec(7, Termios::ODD, 1));
        _serialWordSpecList.push_back(WordSpec(8, Termios::NONE, 1));

        setDeviceName("/tmp/ttyUSB0");
        SerialPortIODevice* pIODevice = new MockSerialPortIODevice(getDeviceName(), _desiredPortConfig);
        setIODevice(pIODevice);
        setSerialPortIODevice(pIODevice);
    }

    ~MockSerialSensor() {}

    virtual IODevice* buildIODevice() throw(nidas::util::IOException)
    {
        DLOG(("TestSerialSensor::buildIODevice(): instantiating TestSerialPortIODevice()"));
        setSerialPortIODevice(new MockSerialPortIODevice(getDeviceName(), _desiredPortConfig));
        if (!getSerialPortIODevice()) {
            throw Exception("SerialSensor::buildIODevice(): failed to instantiate SerialPortIODevice object.");
        }

        // update desiredPortConfig with the port ID data which is populated in the SerialPortIODevice ctor
        // this is needed for future comparisons.
        _desiredPortConfig = getPortConfig();

        return getSerialPortIODevice();
    }

    void open(int flags) throw (nidas::util::IOException, nidas::util::InvalidParameterException)
    {
        getSerialPortIODevice()->open(flags);
    }

    /**
     * Get/set the working PortConfig - this means the ones in SerialPortIODevice and SerialXcvrCtrl, if they exist.
     *
     * Assumption: If SerialSensor is a traditional, RS232/422/485, device, then the associated PortConfig in
     *             SerialPortIODevice is all that matters. This should cover "non-sensor" devices such as GPS, cell
     *             modems, etc, since they may be traditional serial devices, but not have a SerialXcvrCtrl object
     *             associated with them.
     */
    PortConfig getPortConfig() {return SerialSensor::getDefaultPortConfig(); }
    void setPortConfig(const PortConfig newPortConfig){SerialSensor::setPortConfig(newPortConfig);}
    void applyPortConfig() {SerialSensor::applyPortConfig();}
    void printPortConfig(bool flush=true){SerialSensor::printPortConfig(flush);}

    AUTOCONFIG_STATE getAutoConfigState() {return SerialSensor::getAutoConfigState(); }
    AUTOCONFIG_STATE getSerialConfigState() {return SerialSensor::getSerialConfigState(); }
    AUTOCONFIG_STATE getScienceConfigState() {return SerialSensor::getScienceConfigState(); }
    static std::string autoCfgToStr(AUTOCONFIG_STATE autoState){return SerialSensor::autoCfgToStr(autoState);}

    PortConfig getDefaultPortConfig() {return SerialSensor::getDefaultPortConfig();}
    PortConfig getDesiredPortConfig() {return SerialSensor::getDesiredPortConfig();}

    /**
     * autoconfig specific methods
     */
    void doAutoConfig() {SerialSensor::doAutoConfig();}
    void setTargetPortConfig(PortConfig& target, int baud, int dataBits, Termios::parity parity, int stopBits,
                                                 int rts485, PORT_TYPES portType, TERM termination,
                                                 SENSOR_POWER_STATE power);
    bool isDefaultConfig(const PortConfig& rTestConfig) const;
    bool findWorkingSerialPortConfig() {return SerialSensor::findWorkingSerialPortConfig();}
    bool testDefaultPortConfig();
    bool sweepCommParameters();
    std::size_t readEntireResponse(void *buf, std::size_t len, int msecTimeout);
    std::size_t readResponse(void *buf, std::size_t len, int msecTimeout);
    bool doubleCheckResponse();
    bool configureScienceParameters();

    /**
     * These autoconfig methods do nothing, unless overridden in a subclass
     * Keep in mind that most subclasses will override all of these methods.
     * In particular, supportsAutoConfig() must be overridden to return true,
     * otherwise, the other virtual methods will not get called at all.
     */
//    virtual CFG_MODE_STATUS enterConfigMode() { return ENTERED; }
//    virtual CFG_MODE_STATUS getConfigMode() { return _cfgMode; }
//    virtual void setConfigMode(CFG_MODE_STATUS newCfgMode) { _cfgMode = newCfgMode; }
//    virtual void exitConfigMode() {}
    virtual bool supportsAutoConfig() { return true; }
    virtual bool checkResponse() { return getPortConfig() == deviceOperatingPortConfig; }
    virtual bool installDesiredSensorConfig(const PortConfig& /*rDesiredConfig*/) { return true; };
    virtual void sendScienceParameters() {}
    virtual bool checkScienceParameters() { return true; }

    // some test helpers
    WordSpecList& getWordSpecList() {return _serialWordSpecList;}
    BaudRateList& getBaudRateList() {return _baudRateList;}
    PortTypeList& getPortTypeList() {return _portTypeList;}

    MessageConfig defaultMessageConfig;
};

#endif //TEST_SERIAL_SENSOR_HPP
