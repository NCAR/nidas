#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#define BOOST_TEST_MODULE SerialSensorAutoConfig
#include <boost/test/auto_unit_test.hpp>
//#include <pre/boost/asio/mockup_serial_port_service.hpp>

using boost::unit_test_framework::test_suite;

#include <boost/regex.hpp>

#include "NidasApp.h"
#include "MockSerialSensor.h"

#include <sys/types.h>
#include <signal.h>

using namespace nidas::util;
using namespace nidas::core;

PortConfig defaultPortConfig(19200, 7, Termios::EVEN, 1, RS422, TERM_120_OHM, SENSOR_POWER_OFF, -1, false);
PortConfig deviceOperatingPortConfig(38400, 8, Termios::NONE, 1, RS232, NO_TERM, SENSOR_POWER_ON, 0, false);

struct Fixture {
    Fixture()
    {
        logger = Logger::getInstance();
        scheme = logger->getScheme("autoconfig_default");
        LogConfig lc("level=verbose");
        scheme.addConfig(lc);
        logger->setScheme(scheme);

//        SerialPort::createPtyLink("/dev/ttyUSB0");
    }

    ~Fixture()
    {

    }

    Logger* logger;
    LogScheme scheme;

};

BOOST_FIXTURE_TEST_SUITE(autoconfig_test_suite, Fixture)

BOOST_AUTO_TEST_CASE(test_serialsensor_ctors)
{
    MockSerialSensor ss;

    BOOST_TEST_MESSAGE("Testing SerialSensor Constructors...");

    BOOST_TEST(ss.supportsAutoConfig() == true);
    BOOST_TEST(ss.getName().c_str() == "unknown:/tmp/ttyUSB0");
    BOOST_TEST(ss.getAutoConfigState() == AUTOCONFIG_UNSUPPORTED);
    BOOST_TEST(ss.getSerialConfigState() == AUTOCONFIG_UNSUPPORTED);
    BOOST_TEST(ss.getScienceConfigState() == AUTOCONFIG_UNSUPPORTED);
    BOOST_TEST(ss.getDefaultPortConfig() == ss.getDesiredPortConfig());
    BOOST_TEST(ss.getDefaultPortConfig().termios.getBaudRate() == 9600);
    BOOST_TEST(ss.getDefaultPortConfig().termios.getDataBits() == 8);
    BOOST_TEST(ss.getDefaultPortConfig().termios.getParity() == Termios::NONE);
    BOOST_TEST(ss.getDefaultPortConfig().termios.getStopBits() == 1);
    BOOST_TEST(ss.getDefaultPortConfig().xcvrConfig.port == PORT0);
    BOOST_TEST(ss.getDefaultPortConfig().xcvrConfig.portType == RS232);
    BOOST_TEST(ss.getDefaultPortConfig().xcvrConfig.sensorPower == SENSOR_POWER_ON);
    BOOST_TEST(ss.getDefaultPortConfig().xcvrConfig.termination == NO_TERM);

    MockSerialSensor ssArg(defaultPortConfig);

    BOOST_TEST(ssArg.supportsAutoConfig() == true);
    BOOST_TEST(ssArg.getName().c_str() == "unknown:/tmp/ttyUSB0");
    BOOST_TEST(ssArg.getAutoConfigState() == AUTOCONFIG_UNSUPPORTED);
    BOOST_TEST(ssArg.getSerialConfigState() == AUTOCONFIG_UNSUPPORTED);
    BOOST_TEST(ssArg.getScienceConfigState() == AUTOCONFIG_UNSUPPORTED);
    BOOST_TEST(ssArg.getDefaultPortConfig() == ssArg.getDesiredPortConfig());
    BOOST_TEST(ssArg.getDefaultPortConfig().termios.getBaudRate() == 19200);
    BOOST_TEST(ssArg.getDefaultPortConfig().termios.getDataBits() == 7);
    BOOST_TEST(ssArg.getDefaultPortConfig().termios.getParity() == Termios::EVEN);
    BOOST_TEST(ssArg.getDefaultPortConfig().termios.getStopBits() == 1);
    BOOST_TEST(ssArg.getDefaultPortConfig().xcvrConfig.port == PORT0);
    BOOST_TEST(ssArg.getDefaultPortConfig().xcvrConfig.portType == RS485_FULL);
    BOOST_TEST(ssArg.getDefaultPortConfig().xcvrConfig.sensorPower == SENSOR_POWER_OFF);
    BOOST_TEST(ssArg.getDefaultPortConfig().xcvrConfig.termination == TERM_120_OHM);
}

BOOST_AUTO_TEST_CASE(test_serialsensor_findWorkingSerialPortConfig)
{
    BOOST_TEST_MESSAGE("Testing SerialSensor::findWorkingSerialPortConfig()...");

    // default port config different from sensor operating config
    MockSerialSensor ssArg(defaultPortConfig);
    ssArg.open(O_RDWR);
    BOOST_TEST(ssArg.findWorkingSerialPortConfig() == true);
    BOOST_TEST(ssArg.getPortConfig() == deviceOperatingPortConfig);

//    // default port config same as sensor operating config
//    TestSerialSensor ssArg2(defaultPortConfig);
//    deviceOperatingPortConfig = defaultPortConfig;
//    BOOST_TEST(ssArg2.findWorkingSerialPortConfig() == true);
//    BOOST_TEST(ssArg2.getPortConfig() == deviceOperatingPortConfig);
//
//    // Sensor operating config not in allowed port configs
//    TestSerialSensor ssArg3(defaultPortConfig);
//    deviceOperatingPortConfig.termios.setBaudRate(115200);
//    BOOST_TEST(ssArg3.findWorkingSerialPortConfig() == false);
//    BOOST_TEST(ssArg3.getPortConfig() != deviceOperatingPortConfig);
}

BOOST_AUTO_TEST_SUITE_END()

