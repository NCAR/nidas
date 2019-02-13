
#define BOOST_TEST_DYN_LINK
//#define BOOST_AUTO_TEST_MAIN
//#define BOOST_TEST_MODULE SerialSensorAutoConfig
#include <boost/test/unit_test.hpp>

using boost::unit_test_framework::test_suite;

#include <boost/regex.hpp>

#include "nidas/core/Project.h"
#include <nidas/core/DSMConfig.h>
#include "nidas/dynld/DSMSerialSensor.h"
#include "MockSerialSensor.h"
#include "nidas/util/SerialPort.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#define UNIT_TEST_DEBUG_LOG 0

using namespace nidas::util;
using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::isff;

extern bool autoConfigUnitTest;

class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
    Project& operator()() {return *Project::getInstance();}
};

PortConfig _defaultPortConfig(19200, 7, Termios::EVEN, 1, RS422, TERM_120_OHM,-1, false);
PortConfig defaultPortConfig(_defaultPortConfig);
PortConfig _deviceOperatingPortConfig(38400, 8, Termios::NONE, 1, RS232, NO_TERM, 0, false);
PortConfig deviceOperatingPortConfig(_deviceOperatingPortConfig);

void resetPortConfigs()
{
    defaultPortConfig = _defaultPortConfig;
    deviceOperatingPortConfig = _deviceOperatingPortConfig;
}

void cleanup(int /*signal*/)
{
    if (errno != 0)
        perror("Socat child process died and became zombified!");
    else
        perror("Waiting for socat child process to exit...");

    while (waitpid(-1, (int*)0, WNOHANG) > 0) {}
}

struct Fixture {
    Fixture()
    {
#if UNIT_TEST_DEBUG_LOG != 0
        Logger* logger = Logger::getInstance();
        LogScheme scheme = logger->getScheme("autoconfig_default");
        LogConfig lc("level=verbose");
        scheme.addConfig(lc);
        logger->setScheme(scheme);
#endif //UNIT_TEST_DEBUG_LOG != 0

        // Needs to be set up same as SerialSensor::SerialSensor() ctors
        // in order for mocked checkResponse() to work.
        _defaultPortConfig.termios.setRaw(true);
        _defaultPortConfig.termios.setRawLength(1);
        _defaultPortConfig.termios.setRawTimeout(0);
        _deviceOperatingPortConfig.termios.setRaw(true);
        _deviceOperatingPortConfig.termios.setRawLength(1);
        _deviceOperatingPortConfig.termios.setRawTimeout(0);
    }

    ~Fixture() {}
};

bool init_unit_test()
{
    autoConfigUnitTest = true;
    int fd = nidas::util::SerialPort::createPtyLink("/tmp/dev/ttyDSM0");
    if (fd < 0) {
        std::cerr << "Failed to create a Pty for use in autoconfig unit tests." << std::endl;
        return false;
    }

    return true;
}

// entry point:
int main(int argc, char* argv[])
{
    int retval = boost::unit_test::unit_test_main( &init_unit_test, argc, argv );

    return retval;
}


BOOST_FIXTURE_TEST_SUITE(autoconfig_test_suite, Fixture)

BOOST_AUTO_TEST_CASE(test_serialsensor_ctors)
{
    BOOST_TEST_MESSAGE("");
    BOOST_TEST_MESSAGE("Testing SerialSensor Constructors...");

    resetPortConfigs();

    BOOST_TEST_MESSAGE("    Testing SerialSensor Default Constructor...");
    SerialSensor ss;

    BOOST_TEST(ss.getName().c_str() == "unknown:");
    BOOST_TEST(ss.getAutoConfigState() == AUTOCONFIG_UNSUPPORTED);
    BOOST_TEST(ss.getSerialConfigState() == AUTOCONFIG_UNSUPPORTED);
    BOOST_TEST(ss.getScienceConfigState() == AUTOCONFIG_UNSUPPORTED);
    BOOST_TEST(ss.getDefaultPortConfig() == ss.getDesiredPortConfig());
    BOOST_TEST(ss.getDefaultPortConfig().termios.getBaudRate() == 9600);
    BOOST_TEST(ss.getDefaultPortConfig().termios.getDataBits() == 8);
    BOOST_TEST(ss.getDefaultPortConfig().termios.getParity() == Termios::NONE);
    BOOST_TEST(ss.getDefaultPortConfig().termios.getStopBits() == 1);
    BOOST_TEST(ss.getDefaultPortConfig().xcvrConfig.port == SER_PORT0);
    BOOST_TEST(ss.getDefaultPortConfig().xcvrConfig.portType == RS232);
    BOOST_TEST(ss.getDefaultPortConfig().xcvrConfig.termination == NO_TERM);

    BOOST_TEST_MESSAGE("    Testing AutoConfig MockSerialSensor Constructor which passes default PortConfig arg to SerialSensor...");
    MockSerialSensor mss;

    BOOST_TEST(mss.getAutoConfigSupport() == true);
    BOOST_TEST(mss.getName().c_str() == "unknown:");
    BOOST_TEST(mss.getAutoConfigState() == WAITING_IDLE);
    BOOST_TEST(mss.getSerialConfigState() == WAITING_IDLE);
    BOOST_TEST(mss.getScienceConfigState() == WAITING_IDLE);
    BOOST_TEST(mss.getDefaultPortConfig() == mss.getDesiredPortConfig());
    BOOST_TEST(mss.getDefaultPortConfig().termios.getBaudRate() == 19200);
    BOOST_TEST(mss.getDefaultPortConfig().termios.getDataBits() == 7);
    BOOST_TEST(mss.getDefaultPortConfig().termios.getParity() == Termios::EVEN);
    BOOST_TEST(mss.getDefaultPortConfig().termios.getStopBits() == 1);
    BOOST_TEST(mss.getDefaultPortConfig().xcvrConfig.port == SER_PORT0);
    BOOST_TEST(mss.getDefaultPortConfig().xcvrConfig.portType == RS485_FULL);
    BOOST_TEST(mss.getDefaultPortConfig().xcvrConfig.termination == TERM_120_OHM);
}

BOOST_AUTO_TEST_CASE(test_serialsensor_findWorkingSerialPortConfig)
{
    BOOST_TEST_MESSAGE("Testing SerialSensor::findWorkingSerialPortConfig()...");

    resetPortConfigs();

    BOOST_TEST_MESSAGE("    Default port config different from sensor operating config.");
    BOOST_TEST_MESSAGE("        causes sweepCommParameters() to be invoked.");

    MockSerialSensor mss;

    mss.setDeviceName("/tmp/dev/ttyDSM0");
    mss.open(O_RDWR);
    BOOST_TEST(mss.getDefaultPortConfig() == defaultPortConfig);

    BOOST_TEST_MESSAGE("    Default port config same as sensor operating config.");

    defaultPortConfig = deviceOperatingPortConfig;

    MockSerialSensor mss2;
    mss2.setDeviceName("/tmp/dev/ttyDSM0");
    mss2.open(O_RDWR);
    BOOST_TEST(mss2.getDefaultPortConfig() == deviceOperatingPortConfig);

    BOOST_TEST_MESSAGE("    Sensor operating config not in allowed port configs.");
    BOOST_TEST_MESSAGE("        causes sweepCommParameters() to be invoked.");
    MockSerialSensor mss3;
    mss3.setDeviceName("/tmp/dev/ttyDSM0");
    deviceOperatingPortConfig.termios.setBaudRate(115200);
    mss3.open(O_RDWR);
    BOOST_TEST(mss3.getPortConfig() != deviceOperatingPortConfig);

    FtdiGpio<FTDI_GPIO, INTERFACE_A>::deleteFtdiGpio();
}

BOOST_AUTO_TEST_CASE(test_serialsensor_fromDOMElement)
{
    BOOST_TEST_MESSAGE("");
    BOOST_TEST_MESSAGE("Testing SerialSensor::fromDOMElement()...");

    struct stat statbuf;
    std::string xmlFileName = "";

    resetPortConfigs();

    {
        AutoProject ap;

        BOOST_TEST_MESSAGE("    Testing Legacy DSMSerialSensor Dynld Implementation");
        xmlFileName = "legacy_autoconfig.xml";
        BOOST_TEST_REQUIRE(::stat(xmlFileName.c_str(),&statbuf) == 0);

        ap().parseXMLConfigFile(xmlFileName);
        DSMConfigIterator di = ap().getDSMConfigIterator();
        BOOST_REQUIRE(di.hasNext() == true);
        DSMConfig* pDsm = const_cast<DSMConfig*>(di.next());
        (*pDsm).validate();

        SensorIterator sensIter = ap().getSensorIterator();
        BOOST_REQUIRE(sensIter.hasNext() == true );
        SerialSensor* pSerialSensor = dynamic_cast<SerialSensor*>(sensIter.next());
        BOOST_REQUIRE(pSerialSensor != 0);
        // validate that the sensor really is a legacy DSMSerialSensor...
        DSMSerialSensor* pDSMSerialSensor = dynamic_cast<DSMSerialSensor*>(pSerialSensor);
        BOOST_TEST(pDSMSerialSensor != static_cast<DSMSerialSensor*>(0));
        pSerialSensor->init();
        // Don't open/close because the pty being mocked can't handle termios details
        PortConfig desiredPortConfig = pSerialSensor->getDesiredPortConfig();
        BOOST_TEST(desiredPortConfig != pSerialSensor->getDefaultPortConfig());
        BOOST_TEST(desiredPortConfig.termios.getBaudRate() == 9600);
        BOOST_TEST(desiredPortConfig.termios.getParity() == Termios::EVEN);
        BOOST_TEST(desiredPortConfig.termios.getDataBits() == 7);
        BOOST_TEST(desiredPortConfig.termios.getStopBits() ==1);
        BOOST_TEST(pSerialSensor->getInitString() == "DSMSensorTestInitString");
    }

    {
        AutoProject ap;

        BOOST_TEST_MESSAGE("    Testing MockSerialSensor - No <serialSensor> tag attributes...");
        xmlFileName = "autoconfig0.xml";
        BOOST_TEST_REQUIRE(::stat(xmlFileName.c_str(),&statbuf) == 0);

        ap().parseXMLConfigFile(xmlFileName);
        DSMConfigIterator di = ap().getDSMConfigIterator();
        BOOST_REQUIRE(di.hasNext() == true);
        DSMConfig* pDsm = const_cast<DSMConfig*>(di.next());
        (*pDsm).validate();

        SensorIterator sensIter = ap().getSensorIterator();
        BOOST_REQUIRE(sensIter.hasNext() == true );
        MockSerialSensor* pMockSerialSensor = dynamic_cast<MockSerialSensor*>(sensIter.next());
        BOOST_TEST(pMockSerialSensor != static_cast<MockSerialSensor*>(0));
        pMockSerialSensor->init();
        // Don't open/close because the pty being for mock can't handle termios details
        PortConfig desiredPortConfig = pMockSerialSensor->getDesiredPortConfig();
        BOOST_TEST(desiredPortConfig == pMockSerialSensor->getDefaultPortConfig());
        BOOST_TEST(desiredPortConfig.termios.getBaudRate() == 19200);
        BOOST_TEST(desiredPortConfig.termios.getParity() == Termios::EVEN);
        BOOST_TEST(desiredPortConfig.termios.getDataBits() == 7);
        BOOST_TEST(desiredPortConfig.termios.getStopBits() ==1);
        BOOST_TEST(pMockSerialSensor->getInitString() == "MockSensor0TestInitString");
    }

    {
        AutoProject ap;

        BOOST_TEST_MESSAGE("    Testing MockSerialSensor - Use <serialSensor> tag attributes");
        xmlFileName = "autoconfig1.xml";
        BOOST_TEST_REQUIRE(::stat(xmlFileName.c_str(),&statbuf) == 0);

        ap().parseXMLConfigFile(xmlFileName);
        DSMConfigIterator di = ap().getDSMConfigIterator();
        BOOST_REQUIRE(di.hasNext() == true);
        DSMConfig* pDsm = const_cast<DSMConfig*>(di.next());
        (*pDsm).validate();

        SensorIterator sensIter = ap().getSensorIterator();
        BOOST_REQUIRE(sensIter.hasNext() == true );
        MockSerialSensor* pMockSerialSensor = dynamic_cast<MockSerialSensor*>(sensIter.next());
        BOOST_TEST(pMockSerialSensor != static_cast<MockSerialSensor*>(0));
        pMockSerialSensor->init();
        // Don't open/close because the pty being for mock can't handle termios details
        PortConfig desiredPortConfig = pMockSerialSensor->getDesiredPortConfig();
        BOOST_TEST(desiredPortConfig != pMockSerialSensor->getDefaultPortConfig());
        BOOST_TEST(desiredPortConfig.termios.getBaudRate() == 9600);
        BOOST_TEST(desiredPortConfig.termios.getParity() == Termios::EVEN);
        BOOST_TEST(desiredPortConfig.termios.getDataBits() == 7);
        BOOST_TEST(desiredPortConfig.termios.getStopBits() ==1);
        BOOST_TEST(pMockSerialSensor->getInitString() == "MockSensor1TestInitString");
    }

    {
        AutoProject ap;

        BOOST_TEST_MESSAGE("    Testing MockSerialSensor - Use <autoconfig> tag attributes");
        xmlFileName = "autoconfig2.xml";
        BOOST_TEST_REQUIRE(::stat(xmlFileName.c_str(),&statbuf) == 0);

        ap().parseXMLConfigFile(xmlFileName);
        DSMConfigIterator di = ap().getDSMConfigIterator();
        BOOST_REQUIRE(di.hasNext() == true);
        DSMConfig* pDsm = const_cast<DSMConfig*>(di.next());
        (*pDsm).validate();

        SensorIterator sensIter = ap().getSensorIterator();
        BOOST_REQUIRE(sensIter.hasNext() == true );
        MockSerialSensor* pMockSerialSensor = dynamic_cast<MockSerialSensor*>(sensIter.next());
        BOOST_TEST(pMockSerialSensor != static_cast<MockSerialSensor*>(0));
        pMockSerialSensor->init();
        // Don't open/close because the pty being for mock can't handle termios details
        PortConfig desiredPortConfig = pMockSerialSensor->getDesiredPortConfig();
        BOOST_TEST(desiredPortConfig != pMockSerialSensor->getDefaultPortConfig());
        BOOST_TEST(desiredPortConfig.termios.getBaudRate() == 19200);
        BOOST_TEST(desiredPortConfig.termios.getParity() == Termios::ODD);
        BOOST_TEST(desiredPortConfig.termios.getDataBits() == 8);
        BOOST_TEST(desiredPortConfig.termios.getStopBits() == 2);
        BOOST_TEST(pMockSerialSensor->getInitString() == "MockSensor2TestInitString");
    }
}

BOOST_AUTO_TEST_SUITE_END()

