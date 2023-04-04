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
    BOOST_CHECK_EQUAL(hwi->lookupDevice(PORT0).id(), "port0");
    BOOST_CHECK_EQUAL(hwi->lookupDevice("1").id(), "port1");
    BOOST_CHECK_EQUAL(hwi->lookupDevice("DCDC").id(), "dcdc");
    BOOST_CHECK_EQUAL(hwi->lookupDevice("/dev/ttyDSM1").id(), "port1");
    BOOST_CHECK_EQUAL(hwi->lookupDevice("/tmp/ttyDSM1").id(), "port1");
    BOOST_CHECK_EQUAL(hwi->lookupDevice("/dev/ttyDSM12").id(), "");

    // check that other aliases work
    BOOST_CHECK_EQUAL(DEF.id(), P1.id());
    BOOST_CHECK_EQUAL(HardwareDevice(DEF).id(), P1.id());
    BOOST_CHECK_EQUAL(hwi->lookupDevice("def").id(), P1.id());
    BOOST_CHECK_EQUAL(hwi->lookupDevice("DEF").id(), P1.id());
}



BOOST_AUTO_TEST_CASE(test_null_hardware)
{
    // test the hwi short-circuit for unknown hardware.  looking up an unknown
    // device should not create a hardware implementation.
    HardwareInterface::resetInterface();

    auto device = HardwareDevice::lookupDevice("/dev/ttyUSB1");
    BOOST_CHECK(!device.hasReference());
    BOOST_CHECK(!device.iSerial());
    BOOST_CHECK(!device.iOutput());
    BOOST_CHECK(!device.iButton());
    BOOST_CHECK(!device.hasReference());
    device = HardwareDevice::lookupDevice(DCDC);
    BOOST_CHECK(device.hasReference());
}


BOOST_AUTO_TEST_CASE(test_port_type)
{
    PortType pt;
    BOOST_CHECK(pt.parse("232"));
    BOOST_CHECK_EQUAL(RS232, pt);

    PortType p2(pt);
    BOOST_CHECK_EQUAL(RS232, pt);
    BOOST_CHECK_EQUAL(RS422.toLongString(), "RS422");
    BOOST_CHECK_EQUAL(RS422.toLongString(true), "RS485_FULL");
    BOOST_CHECK_EQUAL(RS232.toShortString(), "232");
    BOOST_CHECK_EQUAL(RS485_HALF.toLongString(), "RS485_HALF");
    BOOST_CHECK_EQUAL(RS485_FULL.toShortString(), "422");
    BOOST_CHECK_EQUAL(RS485_FULL.toShortString(true), "485f");

    BOOST_CHECK_EQUAL(nidas::core::RS422.toShortString(), "422");
    BOOST_CHECK_EQUAL(nidas::core::RS232.toShortString(), "232");
    BOOST_CHECK_EQUAL(nidas::core::RS485_HALF.toShortString(), "485h");
    BOOST_CHECK_EQUAL(nidas::core::RS485_FULL.toShortString(), "422");

    BOOST_CHECK_EQUAL(PortType::RS422.toShortString(), "422");
    BOOST_CHECK_EQUAL(PortType::RS232.toShortString(), "232");
    BOOST_CHECK_EQUAL(PortType::RS485_HALF.toShortString(), "485h");
    BOOST_CHECK_EQUAL(PortType::RS485_FULL.toShortString(), "422");

    BOOST_CHECK_EQUAL(PortType::RS422.toString(ptf_long), "RS422");
    BOOST_CHECK_EQUAL(RS422.toString(ptf_long, ptf_485), "RS485_FULL");
    BOOST_CHECK_EQUAL(RS232.toString(ptf_long, ptf_485), "RS232");
    BOOST_CHECK_EQUAL(RS232.toString(ptf_485), "232");
    BOOST_CHECK_EQUAL(RS232.toString(ptf_long, ptf_485), "RS232");

    const PortType cpt(PortType::RS232);
    BOOST_CHECK_EQUAL(cpt, PortType::RS232);
}

