
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

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#define UNIT_TEST_DEBUG_LOG 0

using namespace nidas::util;
using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::isff;

static int child_pid = -1;

class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
    Project& operator()() {return *Project::getInstance();}
};

PortConfig defaultPortConfig(19200, 7, Termios::EVEN, 1, RS422, TERM_120_OHM,-1, false);
PortConfig deviceOperatingPortConfig(38400, 8, Termios::NONE, 1, RS232, NO_TERM, 0, false);

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
        logger = Logger::getInstance();
        scheme = logger->getScheme("autoconfig_default");
        LogConfig lc("level=verbose");
        scheme.addConfig(lc);
        logger->setScheme(scheme);

        Logger* logger;
        LogScheme scheme;
#endif //UNIT_TEST_DEBUG_LOG != 0

        // Needs to be set up same as SerialSensor::SerialSensor() ctors
        // in order for mocked checkResponse() to work.
        defaultPortConfig.termios.setRaw(true);
        defaultPortConfig.termios.setRawLength(1);
        defaultPortConfig.termios.setRawTimeout(0);
        deviceOperatingPortConfig.termios.setRaw(true);
        deviceOperatingPortConfig.termios.setRawLength(1);
        deviceOperatingPortConfig.termios.setRawTimeout(0);
    }

    ~Fixture() {}
};

bool init_unit_test()
{
    std::cout << "Initializing unit test by starting socat..." << std::endl << std::flush;
    bool retval = false;

    // start up socat
    // register cleanup handler first
    signal(SIGCHLD, cleanup);
    int pid = fork();
    if (pid == 0) {
        if (execlp("/usr/bin/socat", "-d", "-d",
                   "pty,link=/tmp/ttyUSB0,wait-slave,raw,b9600",
                   "pty,link=/tmp/ttyUSB1,wait-slave,raw,b9600",
                   (char*)NULL) == -1) {
            perror("execlp failed!!");
            return retval;
        }
        exit(EXIT_FAILURE);
    }
    else {
        child_pid = pid;
        sleep(2);
        retval = true;
    }

    return retval;
}

// entry point:
int main(int argc, char* argv[])
{
    int retval = boost::unit_test::unit_test_main( &init_unit_test, argc, argv );

    // assume socat is still running, kill it nicely so it cleans up after itself...
    std::cout << "Tearing unit test down by killing socat..." << std::endl << std::flush;
    kill(child_pid, SIGINT);

    return retval;
}


BOOST_FIXTURE_TEST_SUITE(autoconfig_test_suite, Fixture)

BOOST_AUTO_TEST_CASE(test_serialsensor_ctors)
{
    BOOST_TEST_MESSAGE("");
    BOOST_TEST_MESSAGE("Testing SerialSensor Constructors...");

    BOOST_TEST_MESSAGE("    Testing Default Constructor...");
    MockSerialSensor ss;
    BOOST_TEST(ss.supportsAutoConfig() == true);
    BOOST_TEST(ss.getName().c_str() == "unknown:/tmp/ttyUSB0");
    BOOST_TEST(ss.getAutoConfigState() == WAITING_IDLE);
    BOOST_TEST(ss.getSerialConfigState() == WAITING_IDLE);
    BOOST_TEST(ss.getScienceConfigState() == WAITING_IDLE);
    BOOST_TEST(ss.getDefaultPortConfig() == ss.getDesiredPortConfig());
    BOOST_TEST(ss.getDefaultPortConfig().termios.getBaudRate() == 9600);
    BOOST_TEST(ss.getDefaultPortConfig().termios.getDataBits() == 8);
    BOOST_TEST(ss.getDefaultPortConfig().termios.getParity() == Termios::NONE);
    BOOST_TEST(ss.getDefaultPortConfig().termios.getStopBits() == 1);
    BOOST_TEST(ss.getDefaultPortConfig().xcvrConfig.port == PORT0);
    BOOST_TEST(ss.getDefaultPortConfig().xcvrConfig.portType == RS232);
    BOOST_TEST(ss.getDefaultPortConfig().xcvrConfig.termination == NO_TERM);

    BOOST_TEST_MESSAGE("    Testing Constructor with Default PortConfig arg...");
    MockSerialSensor ssArg(defaultPortConfig);

    BOOST_TEST(ssArg.supportsAutoConfig() == true);
    BOOST_TEST(ssArg.getName().c_str() == "unknown:/tmp/ttyUSB0");
    BOOST_TEST(ssArg.getAutoConfigState() == WAITING_IDLE);
    BOOST_TEST(ssArg.getSerialConfigState() == WAITING_IDLE);
    BOOST_TEST(ssArg.getScienceConfigState() == WAITING_IDLE);
    BOOST_TEST(ssArg.getDefaultPortConfig() == ssArg.getDesiredPortConfig());
    BOOST_TEST(ssArg.getDefaultPortConfig().termios.getBaudRate() == 19200);
    BOOST_TEST(ssArg.getDefaultPortConfig().termios.getDataBits() == 7);
    BOOST_TEST(ssArg.getDefaultPortConfig().termios.getParity() == Termios::EVEN);
    BOOST_TEST(ssArg.getDefaultPortConfig().termios.getStopBits() == 1);
    BOOST_TEST(ssArg.getDefaultPortConfig().xcvrConfig.port == PORT0);
    BOOST_TEST(ssArg.getDefaultPortConfig().xcvrConfig.portType == RS485_FULL);
    BOOST_TEST(ssArg.getDefaultPortConfig().xcvrConfig.termination == TERM_120_OHM);
}

BOOST_AUTO_TEST_CASE(test_serialsensor_findWorkingSerialPortConfig)
{
    BOOST_TEST_MESSAGE("Testing SerialSensor::findWorkingSerialPortConfig()...");

    BOOST_TEST_MESSAGE("    Default port config different from sensor operating config.");
    BOOST_TEST_MESSAGE("        causes sweepCommParameters() to be invoked.");
    MockSerialSensor ssArg(defaultPortConfig);
    ssArg.open(O_RDWR);
    BOOST_TEST(ssArg.findWorkingSerialPortConfig() == true);
    BOOST_TEST(ssArg.getPortConfig() == deviceOperatingPortConfig);

    BOOST_TEST_MESSAGE("    Default port config same as sensor operating config.");
    defaultPortConfig = deviceOperatingPortConfig;
    MockSerialSensor ssArg2(defaultPortConfig);
    BOOST_TEST(ssArg2.findWorkingSerialPortConfig() == true);
    BOOST_TEST(ssArg2.getPortConfig() == deviceOperatingPortConfig);

    BOOST_TEST_MESSAGE("    Sensor operating config not in allowed port configs.");
    BOOST_TEST_MESSAGE("        causes sweepCommParameters() to be invoked.");
    MockSerialSensor ssArg3(defaultPortConfig);
    deviceOperatingPortConfig.termios.setBaudRate(115200);
    BOOST_TEST(ssArg3.findWorkingSerialPortConfig() == false);
    BOOST_TEST(ssArg3.getPortConfig() != deviceOperatingPortConfig);
}

BOOST_AUTO_TEST_CASE(test_serialsensor_fromDOMElement)
{
    BOOST_TEST_MESSAGE("");
    BOOST_TEST_MESSAGE("Testing SerialSensor::fromDOMElement()...");

    struct stat statbuf;
    std::string xmlFileName = "";

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
        BOOST_TEST(desiredPortConfig.termios.getBaudRate() == 9600);
        BOOST_TEST(desiredPortConfig.termios.getParity() == Termios::NONE);
        BOOST_TEST(desiredPortConfig.termios.getDataBits() == 8);
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

