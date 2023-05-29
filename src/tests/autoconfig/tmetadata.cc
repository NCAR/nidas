// -*- mode: C++; c-basic-offset: 4 -*-

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using boost::unit_test_framework::test_suite;

#ifdef STANDALONE
#include <nidas/core/Metadata.cc>
#else
#include <nidas/dynld/isff/NCAR_TRH.h>
using nidas::dynld::isff::NCAR_TRH;
#endif
#include <nidas/core/Metadata.h>


#include <nidas/util/Logger.h>

#define UNIT_TEST_DEBUG_LOG 0

using namespace nidas::util;
using namespace nidas::core;


bool init_unit_test()
{
    return true;
}

// entry point:
int main(int argc, char* argv[])
{
    // nidas::core::NidasApp napp(argv[0]);
    // napp.enableArguments(napp.loggingArgs());
    // napp.allowUnrecognized(true);
    // ArgVector args = napp.parseArgs(argc, argv);
    auto logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(LogConfig("debug")));
    DLOG(("main entered, args parsed, debugging enabled..."));
    int retval = boost::unit_test::unit_test_main( &init_unit_test, argc, argv );
    return retval;
}


class MetadataTest: public SensorMetadata
{
public:
    MetadataTest(): SensorMetadata("Test")
    {}

    MetadataDouble number{this, READWRITE, "number", "This is a floating point number."};
    MetadataBool flag{this, READWRITE, "flag"};
    MetadataDouble positive{this, READWRITE, "positive", "Positive", 4, 0.0};
    MetadataInt dice{this, READWRITE, "dice", "Dice", 1, 1, 6};
    MetadataDouble pi{this, READWRITE, "pi", "Pi with precision 3", 3};

protected:

    virtual MetadataInterface*
    clone() const override
    {
        return new MetadataTest();
    }

#ifdef notdef
    MetadataTest(const MetadataTest&) = default;

    void enumerate(item_list& items) override
    {
        items.push_back(&number);
        items.push_back(&flag);
        items.push_back(&positive);
        items.push_back(&dice);
        items.push_back(&pi);
    }

    // Make it explicit that this dictionary supports assignment and copy
    // construction.
    MetadataTest& operator=(const MetadataTest&) = default;
    MetadataTest(const MetadataTest&) = default;
#endif
};


BOOST_AUTO_TEST_CASE(test_metadata_simple)
{
    MetadataTest md;

    BOOST_TEST(md.serial_number.unset());
    BOOST_TEST(md.classname() == "Test");

    BOOST_TEST(md.number.unset());
    md.number = 2.0;
    BOOST_TEST(md.number.get() == 2.0);
    BOOST_TEST((float)md.number == 2.0);
    BOOST_TEST(!md.number.unset());

    md.serial_number = "ABC123";
    BOOST_TEST(md.serial_number.get() == "ABC123");

}


class LinearCalMetadata: public MetadataInterface
{
public:
    MetadataDouble slope{this, MetadataItem::READWRITE, "slope"};
    MetadataDouble offset{this, MetadataItem::READWRITE, "offset"};

    LinearCalMetadata():
        MetadataInterface("linearcal")
    {}

    MetadataInterface* clone() const override
    {
        return new LinearCalMetadata;
    }
};


BOOST_AUTO_TEST_CASE(test_metadata_interfaces)
{
    // It should be possible to get at metadata values from multiple
    // interfaces.
    MetadataTest mdt;

    auto& mdc = *mdt.add_interface(LinearCalMetadata());
    BOOST_TEST(mdc.slope.name() == "slope");
    mdc.slope = 2;
    mdc.offset = 1;

    BOOST_REQUIRE(mdc.lookup("slope"));
    BOOST_REQUIRE(mdc.lookup("offset"));
    BOOST_TEST(mdc.lookup("slope")->string_value() == "2");
    BOOST_TEST(mdc.lookup("offset")->string_value() == "1");

    // should also be able to retrieve them from "null" interface, except the
    // string values will be json syntax without the precision setting.
    BOOST_TEST(mdt.string_value("slope") == "2.0");
    BOOST_TEST(mdt.string_value("offset") == "1.0");
}


BOOST_AUTO_TEST_CASE(test_metadata_assign)
{
    MetadataTest md;

    md.number = 2.0;
    md.serial_number = "ABC123";

    MetadataTest md2;
    md2 = md;

    BOOST_TEST(!md2.number.unset());
    BOOST_TEST(!md2.serial_number.unset());
    BOOST_TEST(md2.number.get() == 2.0);
    BOOST_TEST(md2.serial_number.get() == "ABC123");

}

BOOST_AUTO_TEST_CASE(test_metadata_bool)
{
    MetadataTest md;

    BOOST_TEST(md.flag.unset());
    BOOST_TEST(md.flag.get() == false);
    md.flag = false;
    BOOST_TEST(!md.flag.unset());
    BOOST_TEST(md.flag.get() == false);
    md.flag = true;
    BOOST_TEST(!md.flag.unset());
    BOOST_TEST(md.flag.get() == true);

    // BOOST_TEST(md.flag.error().empty());
    md.flag.check_assign_string("x");
    // BOOST_TEST(!md.flag.error().empty());
    BOOST_TEST(md.flag.string_value() == "true");
    md.flag.check_assign_string("false");
    // BOOST_TEST(md.flag.error().empty());
    BOOST_TEST(md.flag.get() == false);
}

BOOST_AUTO_TEST_CASE(test_metadata_lookup)
{
    MetadataTest md;

    md.number = 2.0;
    md.serial_number = "ABC123";

    auto mi = md.lookup(md.number.name());
    BOOST_REQUIRE(mi);
    BOOST_TEST(mi->string_value() == "2");
    BOOST_TEST(!md.lookup("x"));
    BOOST_TEST(md.lookup("serial_number")->string_value() == "ABC123");
    BOOST_TEST(md.lookup("calibration_date")->unset());

    // the same should work on a copy.
    MetadataTest md2;
    md2 = md;
    BOOST_TEST(md2.serial_number.get() == "ABC123");
    BOOST_REQUIRE(md2.lookup("number"));
    BOOST_TEST(md2.lookup("number")->string_value() == "2");
    BOOST_TEST(md2.lookup("serial_number")->string_value() == "ABC123");

    // also check assignment
    MetadataTest ct;
    ct = md;
    BOOST_TEST(ct.lookup("number")->string_value() == "2");
    BOOST_TEST(ct.lookup("serial_number")->string_value() == "ABC123");
    BOOST_TEST(ct.number.get() == 2.0);
    BOOST_TEST(ct.number.description() == "This is a floating point number.");

#ifdef TEST_COMPILE
    // should not compile
    Metadata sliced("Slice");
    sliced = md;
    Metadata plain("Plain");
#endif

}

BOOST_AUTO_TEST_CASE(test_metadata_constraints)
{
    MetadataTest md;

    md.pi = 3.1415927;
    BOOST_TEST(!md.pi.unset());
    BOOST_TEST(md.pi.string_value() == "3.14");

    BOOST_TEST(md.dice.unset());
    BOOST_TEST(md.dice.get() == 0);
    //BOOST_TEST(md.dice.error().empty());
    //DLOG(("") << md.dice.error());

    md.dice = 1;
    BOOST_TEST(md.dice.get() == 1);
    BOOST_TEST(md.dice.string_value() == "1");
    BOOST_TEST(!md.dice.unset());
    //BOOST_TEST(md.dice.error().empty());

    BOOST_TEST(md.dice.set(0) == false);
    //BOOST_TEST(!md.dice.error().empty());
    //DLOG(("") << md.dice.error());
    BOOST_TEST(md.dice.get() == 1);

}


BOOST_AUTO_TEST_CASE(test_metadata_clear)
{
    MetadataTest md;

    // should be able to clear values after setting them.
    md.pi = 3.1415927;
    BOOST_TEST(!md.pi.unset());
    BOOST_TEST(md.pi.string_value() == "3.14");
    MetadataTest md2; md2 = md;

    md.pi.erase();
    BOOST_TEST(md.pi.unset());
    BOOST_TEST(md.pi.string_value() == "");

    // make sure an unset value cannot be assigned
    BOOST_TEST(!md2.pi.unset());
    BOOST_TEST(md2.pi.string_value() == "3.14");
    md2.pi = md.pi;
    BOOST_TEST(!md2.pi.unset());
    BOOST_TEST(md2.pi.string_value() == "3.14");

}


BOOST_AUTO_TEST_CASE(test_metadata_output)
{
    MetadataTest md;

    {
        md.pi = 3.1415927;
        std::ostringstream buf;
        buf << md.pi;
        BOOST_TEST(buf.str() == "3.14");
    }
    {
        std::ostringstream buf;
        buf << md.serial_number;
        BOOST_TEST(buf.str() == "");
    }
}


BOOST_AUTO_TEST_CASE(test_metadata_timestamp)
{
    {
        MetadataTest md;

        UTime ut(true, 2023, 4, 15, 12, 30, 59);
        md.timestamp = ut;
        //BOOST_TEST(md.timestamp.error().empty());
        BOOST_TEST(md.timestamp.string_value() == "2023-04-15T12:30:59.000Z");

        md.timestamp = "2023-03-30T09:00:00Z";
        //BOOST_TEST(md.timestamp.error().empty());
        BOOST_TEST(md.timestamp.get() == UTime(true, 2023, 3, 30, 9, 0, 0));
    }
    {
        MetadataTest md;

        // get() on unset returns a default value and no error.
        BOOST_TEST(md.timestamp.get() == UTime(0l));

        UTime ut(true, 2023, 4, 15, 12, 30, 59);
        BOOST_TEST(md.timestamp.set(ut));
    }

}


BOOST_AUTO_TEST_CASE(test_metadata_get)
{
    // get() on unset values should return a default with no error.
    MetadataTest md;

    BOOST_TEST(md.serial_number.get() == "");
    //BOOST_TEST(md.serial_number.error().empty());
    BOOST_TEST(md.number.get() == 0);
    //BOOST_TEST(md.number.error().empty());
    BOOST_TEST(md.pi.get() == 0);
    //BOOST_TEST(md.pi.error().empty());
    BOOST_TEST(md.timestamp.get() == UTime(0l));
    //BOOST_TEST(md.timestamp.error().empty());

}


BOOST_AUTO_TEST_CASE(test_metadata_json)
{
    MetadataTest md;

    md.number = 2.0;
    md.flag = true;
    md.pi = 3.1415927;
    md.serial_number = "ABC123";
    md.firmware_version = "cygnus 'X1' \"alpha\" 42";

    // compact single-line json with no spaces.  the dictionary is in order of
    // assignment rather than in interface order, not sure if that matters.
    std::string xbuf =
    R"""({"firmware_version":"cygnus 'X1' \"alpha\" 42","flag":true,"number":2.0,"pi":3.1415927,"serial_number":"ABC123"})""";
    BOOST_TEST(md.to_buffer() == xbuf);

    xbuf = R"""({
  "firmware_version" : "cygnus 'X1' \"alpha\" 42",
  "flag" : true,
  "number" : 2.0,
  "pi" : 3.1415927,
  "serial_number" : "ABC123"
})""";
    BOOST_TEST(md.to_buffer(2) == xbuf);

    MetadataTest mdf;
    std::cerr << md.to_buffer(2) << std::endl;
    BOOST_TEST(mdf.from_buffer(md.to_buffer()));
    BOOST_TEST(mdf.number.get() == 2.0);
    BOOST_TEST(mdf.flag.get() == true);
    BOOST_TEST(mdf.pi.string_value() == "3.14");
    BOOST_TEST(mdf.serial_number.get() == "ABC123");
    BOOST_TEST(mdf.firmware_version.get() == "cygnus 'X1' \"alpha\" 42");
}

#ifndef STANDALONE
BOOST_AUTO_TEST_CASE(test_trh_metadata_load)
{
    NCAR_TRH trh;

    Metadata md;

    // Load the sensor config from a json string.
    md.load();

    // now how do we check it? get an implementation from the sensor, copied by value
    // but with pointers to the static function which can check it.
    //
    Metadata trhmd = trh.getMetadata();
    // Pass it to the TRH, with a way to 
    // trh.
    trhmd.load();
    // check on errors.  if it passes, update sensor.
    trh.setSensorConfig(trhmd);

}
#endif

// BOOST_AUTO_TEST_SUITE_END()
