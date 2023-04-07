
#ifndef MOCK_SERIAL_SENSOR_HPP
#define MOCK_SERIAL_SENSOR_HPP

#include "nidas/core/SerialSensor.h"
#include "nidas/util/SerialPort.h"

using namespace nidas::core;
namespace n_u = nidas::util;

extern PortConfig defaultPortConfig;
extern PortConfig deviceOperatingPortConfig;

namespace nidas { namespace dynld { namespace isff {
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
        addPortConfig(defaultPortConfig);

        for (int baud: { 4800, 9600, 19200, 38400 })
        {
            for (auto pt: { RS232, RS422, RS485_HALF })
            {
                addPortConfig(PortConfig(baud, 7, Parity::E, 1, pt));
                addPortConfig(PortConfig(baud, 7, Parity::O, 1, pt));
                addPortConfig(PortConfig(baud, 8, Parity::N, 1, pt));
            }
        }

        // We set the defaults at construction,
        // letting the base class modify according to fromDOMElement()
        setMessageParameters(defaultMessageConfig);

        setAutoConfigSupported();
        initAutoConfig();
    }

    ~MockSerialSensor() {}

    void fromDOMElement(const xercesc::DOMElement* node)
        throw(nidas::util::InvalidParameterException) override
    {
        SerialSensor::fromDOMElement(node);
        fromDOMElementAutoConfig(node);
    }

protected:
    MessageConfig defaultMessageConfig;

    virtual bool checkResponse()
    {
        bool retval = (getPortConfig() == deviceOperatingPortConfig);
        if (!retval) {
            DLOG(("Active port config:\n") << getPortConfig());
            DLOG(("Device Operating Config:\n") << deviceOperatingPortConfig);
        }
        return retval;
    }
    virtual bool installDesiredSensorConfig(const PortConfig& /*rDesiredConfig*/) { return true; };
    virtual void sendScienceParameters() {}
    virtual bool checkScienceParameters() { return true; }

};

}}} //namespace nidas { namespace dynld { namespace isff {

NIDAS_CREATOR_FUNCTION_NS(isff, MockSerialSensor)

#endif //MOCK_SERIAL_SENSOR_HPP
