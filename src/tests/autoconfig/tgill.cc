// -*- mode: C++; c-basic-offset: 4 -*-
// Test Gill 2D auto-config features.

#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/dynld/isff/GILL2D.h>

#include <memory>
#include <nidas/core/Metadata.h>

using namespace nidas::dynld::isff;
using namespace nidas::core;

BOOST_AUTO_TEST_CASE(test_gill_outputrate)
{
    std::unique_ptr<GILL2D> gill(new GILL2D());

    BOOST_CHECK_EQUAL(gill->validOutputRate(10), true);
    BOOST_CHECK_EQUAL(gill->validOutputRate(9), false);

    BOOST_CHECK_EQUAL(gill->outputRateFromString("10"), 10);
    BOOST_CHECK_EQUAL(gill->outputRateFromString("8"), 8);
    BOOST_CHECK_EQUAL(gill->outputRateFromString("5"), 5);
    BOOST_CHECK_EQUAL(gill->outputRateFromString("1"), 1);

    BOOST_CHECK_THROW(gill->outputRateFromString("P"), InvalidParameterException);
    BOOST_CHECK_THROW(gill->outputRateFromString("3"), InvalidParameterException);

    BOOST_CHECK_EQUAL(gill->outputRateCommand(10), "P6");
    BOOST_CHECK_EQUAL(gill->outputRateCommand(2), "P3");
    BOOST_CHECK_EQUAL(gill->outputRateCommand(4), "P2");
    std::string cmd = gill->formatSensorCmd(SENSOR_OUTPUT_RATE_CMD,
					    nidas::core::SensorCmdArg(2));
    BOOST_CHECK_EQUAL(cmd, "\r\nP3\r");
    cmd = gill->formatSensorCmd(SENSOR_OUTPUT_RATE_CMD,
					    nidas::core::SensorCmdArg(8));
    BOOST_CHECK_EQUAL(cmd, "\r\nP5\r");

    GILL2D_Metadata md;
    gill->getMetadata(md);
    md.output_rate = 5;
    BOOST_TEST(!md.output_rate.unset());
    BOOST_TEST(md.string_value("output_rate") == "5");
    BOOST_TEST(md.output_rate.get() == 5);

    std::string config =
    " A0 B3 C1 E1 F1 G0000 H1 J1 K1 L1 M2 NA O1 P6 T1 U1 V1 X1 Y1 Z1\r\n";

    bool parsed = gill->parseConfigResponse(config);
    // This returns false until "parsing" is separated from matching
    // "desired" settings.
    BOOST_CHECK_EQUAL(parsed, false);

    std::cerr << "Before update:\n" << md.to_buffer(2) << std::endl;
    // this should update the metadata
    gill->getMetadata(md);
    std::cerr << "After update:\n" << md.to_buffer(2) << std::endl;
    BOOST_TEST(!md.output_rate.unset());
    BOOST_TEST(md.string_value("output_rate") == "10");
    BOOST_TEST(md.output_rate.get() == 10);

    config =
    " A2 B3 C1 E1 F1 G0000 J1 K1 L2 M2 NA O2 P6 T1 U1 V1 X1 Y1 Z1\r\n";

}
