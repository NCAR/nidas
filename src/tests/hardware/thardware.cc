#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <boost/regex.hpp>

#include <nidas/core/HardwareInterface.h>

//using namespace nidas::util;
using namespace nidas::core;


BOOST_AUTO_TEST_CASE(test_interface)
{
    HardwareInterface::selectInterface("mock");
    HardwareInterface* hwi = HardwareInterface::getHardwareInterface();
    // should not be null pointer
    BOOST_CHECK(hwi);
    BOOST_CHECK_EQUAL(hwi->getPath(), "mock");
    auto spi = hwi->getSerialPortInterface(Devices::PORT0);
    // should not be null
    BOOST_CHECK(spi);
    BOOST_CHECK(spi == Devices::PORT0.iSerial());

    // a button is an output and button but not a port
    HardwareDevice p1(Devices::P1);
    BOOST_CHECK(!p1.iSerial());
    BOOST_CHECK(p1.iOutput());
    BOOST_CHECK(p1.iButton());
}


BOOST_AUTO_TEST_CASE(test_lookup)
{
    HardwareInterface* hwi = HardwareInterface::getHardwareInterface();
    BOOST_CHECK_EQUAL(hwi->lookupDevice("PORT0").id(), "port0");
    BOOST_CHECK_EQUAL(hwi->lookupDevice("1").id(), "port1");
    BOOST_CHECK_EQUAL(hwi->lookupDevice("DCDC").id(), "dcdc");
}
