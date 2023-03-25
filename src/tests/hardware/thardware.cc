#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <boost/regex.hpp>

#include <nidas/core/HardwareInterface.h>
#include <nidas/core/HardwareInterfaceImpl.h>
#include <nidas/util/Logger.h>

//using namespace nidas::util;
using namespace nidas::core;
using namespace nidas::core::Devices;
using nidas::util::Logger;
using nidas::util::LogScheme;
using nidas::util::LogConfig;


BOOST_AUTO_TEST_CASE(test_interface)
{
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(LogConfig()));

    HardwareInterface::resetInterface();
    auto hdi = HardwareDeviceImpl(Devices::DCDC, "description");
    auto hdicopy = hdi;
    BOOST_CHECK_EQUAL(hdi._id, hdicopy._id);
    BOOST_CHECK_EQUAL(hdi._description, hdicopy._description);

    std::map<std::string, HardwareDeviceImpl> hdmap;
    hdmap[hdi._id] = hdi;
    hdmap[WIFI.id()] = HardwareDeviceImpl(WIFI, "wifi description");
    BOOST_CHECK_EQUAL(hdmap.size(), 2);
    BOOST_CHECK_EQUAL(hdmap[WIFI.id()]._id, "wifi");
    BOOST_CHECK_EQUAL(hdmap[WIFI.id()]._description, "wifi description");
    // before getting an interface, it should be possible to get default
    // descriptions.
    BOOST_CHECK_EQUAL(DCDC.description(), "DC-DC converter relay");

    HardwareInterface::selectInterface(HardwareInterface::MOCK_INTERFACE);
    auto hwi = HardwareInterface::getHardwareInterface();
    // should not be null pointer
    BOOST_CHECK(hwi);
    BOOST_CHECK_EQUAL(hwi->getPath(), "mock");
    auto spi = hwi->getSerialPortInterface(Devices::PORT0);
    // should not be null
    BOOST_CHECK(spi);
    HardwareDevice port(Devices::PORT0);
    BOOST_CHECK(spi == port.iSerial());

    // a button is an output and button but not a port
    HardwareDevice p1(Devices::P1);
    BOOST_CHECK(!p1.iSerial());
    BOOST_CHECK(p1.iOutput());
    BOOST_CHECK(p1.iButton());

    // further fetches should retrieve the same interface
    BOOST_CHECK_EQUAL(hwi, HardwareInterface::getHardwareInterface());

    // if we reset the interface, and then try to get a bogus interface, we
    // should get a null.
    HardwareInterface::resetInterface();
    HardwareInterface::selectInterface("bogus");
    hwi = HardwareInterface::getHardwareInterface();
    BOOST_CHECK_EQUAL(hwi->getPath(), "null");
}


BOOST_AUTO_TEST_CASE(test_lookup)
{
    HardwareInterface::resetInterface();
    HardwareInterface::selectInterface(HardwareInterface::MOCK_INTERFACE);
    auto hwi = HardwareInterface::getHardwareInterface();
    BOOST_CHECK_EQUAL(hwi->lookupDevice("PORT0").id(), "port0");
    BOOST_CHECK_EQUAL(hwi->lookupDevice("1").id(), "port1");
    BOOST_CHECK_EQUAL(hwi->lookupDevice("DCDC").id(), "dcdc");
    BOOST_CHECK_EQUAL(hwi->lookupDevice("/dev/ttyDSM1").id(), "port1");
    BOOST_CHECK_EQUAL(hwi->lookupDevice("/tmp/ttyDSM1").id(), "port1");
    BOOST_CHECK_EQUAL(hwi->lookupDevice("/dev/ttyDSM12").id(), "");
}
