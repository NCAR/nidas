#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#define BOOST_TEST_MODULE SerialSensorAutoConfig
#include <boost/test/auto_unit_test.hpp>
//#include <pre/boost/asio/mockup_serial_port_service.hpp>

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

class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
    Project& operator()() {return *Project::getInstance();}
};

PortConfig defaultPortConfig(19200, 7, Termios::EVEN, 1, RS422, TERM_120_OHM, SENSOR_POWER_ON, -1, false);
PortConfig deviceOperatingPortConfig(38400, 8, Termios::NONE, 1, RS232, NO_TERM, SENSOR_POWER_ON, 0, false);

void cleanup(int /*signal*/)
{
    perror("socat child process died and became zombified!");
    while (waitpid(-1, (int*)0, WNOHANG) > 0) {}
}

struct Fixture {
    Fixture() : child_pid(-1)
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

    ~Fixture()
    {
        // assume socat is still running...
        kill(child_pid, SIGINT);
    }

    pid_t child_pid;
};

bool init_unit_test()
{
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
        }
        exit(EXIT_FAILURE);
    }
    else {
        child_pid = pid;
        sleep(5);
    }
}

// entry point:
int main(int argc, char* argv[])
{
  return boost::unit_test::unit_test_main( &init_unit_test, argc, argv );
}


BOOST_AUTO_TEST_SUITE(autoconfig_test_suite)

BOOST_AUTO_TEST_CASE(test_serialsensor_ctors)
{
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
    BOOST_TEST(ss.getDefaultPortConfig().xcvrConfig.sensorPower == SENSOR_POWER_ON);
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
    BOOST_TEST(ssArg.getDefaultPortConfig().xcvrConfig.sensorPower == SENSOR_POWER_ON);
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
    BOOST_TEST_MESSAGE("Testing SerialSensor::fromDOMElement()...");

    AutoProject ap;
    struct stat statbuf;
    std::string xmlFileName = "";

    BOOST_TEST_MESSAGE("    Testing Legacy DSMSerialSensor Dynld Implementation");
    xmlFileName = "legacy_autoconfig.xml";
    BOOST_TEST_REQUIRE(::stat(xmlFileName.c_str(),&statbuf) == 0);
    NLOG(("Found XML file: ") << xmlFileName);

    ap().parseXMLConfigFile(xmlFileName);
    DSMConfigIterator di = ap().getDSMConfigIterator();
    while (di.hasNext()) {
        DSMConfig* pDsm = const_cast<DSMConfig*>(di.next());
        (*pDsm).validate();
    }

    NLOG(("Iterating through all the sensors specified in the XML file"));
    SensorIterator sensIter = ap().getSensorIterator();
    while (sensIter.hasNext() ) {
        SerialSensor* pSerialSensor = dynamic_cast<SerialSensor*>(sensIter.next());
        if (!pSerialSensor) {
            NLOG(("Can't auto config a non-serial sensor. Skipping..."));
            continue;
        }
        DSMSerialSensor* pDSMSerialSensor = dynamic_cast<DSMSerialSensor*>(pSerialSensor);
        BOOST_TEST(pDSMSerialSensor != static_cast<DSMSerialSensor*>(0));
        pSerialSensor->init();
        // Don't open/close because the pty being for mock can't handle termios details
    }
}

BOOST_AUTO_TEST_SUITE_END()

