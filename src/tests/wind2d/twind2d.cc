#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/dynld/isff/Wind2D.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/Project.h>
#include <memory>
#include <nidas/util/util.h>

using namespace nidas::util;
using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::isff;

using nidas::util::derive_spd_dir_from_uv;
using nidas::util::derive_uv_from_spd_dir;

// Typical XML to describe a Gill sensor.
const char* gill_xml = R"XML(
    <!-- Gill Wind Observer 2D sonic, RS422 -->
    <serialSensor class="isff.PropVane" ID="GILL_WO"
        devicename="/dev/ttyUSB0" height="0.2m" id="4"
        baud="9600" parity="none" databits="8" stopbits="1"
        timeout="10" init_string="q\r">
        <!-- \0x02A,157,000.08,M,+019.83,00,\0x031F\r\n -->
        <sample id="1" scanfFormat="%*2c,%f,%f,M,%f,%f" rate="10" ttadjust="10">
            <variable name="Dir" units="deg"
                      longname="Gill WindObserver, wind direction"
                      plotrange="$DIR_RANGE" maxValue="990">
                <linear intercept="${OFFSET}"/>
            </variable>
            <variable name="Spd" units="m/s"
                      longname="Wind speed, Gill WindObserver"
                      plotrange="$SPD_RANGE" maxValue="990"/>
            <variable name="Tc" units="degC"
                      longname="Sonic temperature, Gill WindObserver"
                      plotrange="$T_RANGE" maxValue="990"/>
            <variable name="Status" units="" longname="Status, Gill WindObserver"
                      plotrange="0 60"/>
            <variable name="U" units="m/s"
              longname="Wind U component, Gill WindObserver"
              plotrange="$UV_RANGE"/>
            <variable name="V" units="m/s"
              longname="Wind V component, Gill WindObserver"
              plotrange="$UV_RANGE"/>
        </sample>
        <message separator="\r" position="end" length="0"/>
        <parameter type="string" name="orientation" value="${ORIENTATION}"/>
    </serialSensor>
)XML";


template <typename T, typename V>
SampleT<T>&
operator<<(SampleT<T>& sample, V one)
{
    unsigned int newlen = sample.getDataLength() + 1;
    if (sample.getAllocLength() < newlen)
        sample.reallocateData(newlen);
    sample.setDataLength(newlen);
    // Since Sample provides virtual setDataValue() methods for various data
    // types, we have to cast the value to a specific type to avoid ambiguity
    // from the template type deduction.
    sample.setDataValue(newlen - 1, (T)one);
    return sample;
}


SampleT<char>&
operator<<(SampleT<char>& sample, const char* one)
{
    unsigned int newlen = strlen(one) + 1;
    sample.allocateData(newlen);
    sample.setDataLength(newlen);
    strcpy(sample.getDataPtr(), one);
    return sample;
}

SampleT<char>&
operator<<(SampleT<char>& sample, const std::string& one)
{
    return sample << one.c_str();
}


BOOST_AUTO_TEST_CASE(test_make_sample)
{
    {
        SampleT<char> sample;
        sample << "\x02""A,324,001.28,M,+008.10,00,\x03""10\r";
        BOOST_CHECK(sample.getDataLength() == 32);
    }
    {
        SampleT<float> sample;
        sample << 1 << 2 << 3;
        BOOST_CHECK(sample.getDataLength() == 3);
        BOOST_CHECK(sample.getDataValue(0) == 1);
        BOOST_CHECK(sample.getDataValue(1) == 2);
        BOOST_CHECK(sample.getDataValue(2) == 3);
    }
    {
        SampleT<float> sample { 3, 2, 1 };
        BOOST_CHECK(sample.getDataLength() == 3);
        BOOST_CHECK(sample.getDataValue(0) == 3);
        BOOST_CHECK(sample.getDataValue(1) == 2);
        BOOST_CHECK(sample.getDataValue(2) == 1);
    }
    {
        SampleChar sample("\x02""A,324,001.28,M,+008.10,00,\x03""10\r");
        BOOST_CHECK(sample.getDataLength() == 32);
    }
}


void
setup_wind2d(Wind2D& wind, bool flipped=false, float offset=0)
{
    static char env_orientation[128];
    static char env_offset[128];

    sprintf(env_orientation, "ORIENTATION=%s",
            flipped ? "flipped" : "normal");
    sprintf(env_offset, "OFFSET=%f", offset);

    wind.setDSMId(1);
    putenv(env_orientation);
    putenv(env_offset);

    std::string xml = Project::getInstance()->expandString(gill_xml);
    std::unique_ptr<xercesc::DOMDocument> doc(XMLParser::ParseString(xml));
    wind.fromDOMElement(doc->getDocumentElement());
    wind.validate();
    wind.init();

    doc.reset();
    XMLImplementation::terminate();
}


BOOST_AUTO_TEST_CASE(test_wind2d_normal)
{
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(LogConfig()));

    Wind2D wind;
    setup_wind2d(wind);

    BOOST_CHECK(wind.getDSMId() == 1);
    BOOST_CHECK(wind.getSensorId() == 4);

    std::list<const Sample*> results;
    SampleT<char>* samp = 
        getSample("\x02""A,324,001.28,M,+008.10,00,\x03""10\r");
    BOOST_CHECK(wind.process(samp, results));
    samp->freeReference();
    BOOST_CHECK(results.size() == 1);
    const Sample* csamp = results.front();
    BOOST_CHECK_CLOSE_FRACTION(csamp->getDataValue(0), 324, 0.001);
    BOOST_CHECK_CLOSE_FRACTION(csamp->getDataValue(1), 1.28, 0.001);
    BOOST_CHECK_CLOSE_FRACTION(csamp->getDataValue(2), 8.1, 0.001);
    BOOST_CHECK_EQUAL(csamp->getDataValue(3), 0);
    csamp->freeReference();
    SamplePools::deleteInstance();
}


BOOST_AUTO_TEST_CASE(test_uv)
{
    float u = 1;
    float v = 1;
    float dir = 45;
    float spd = 1;

    derive_uv_from_spd_dir(u, v, spd, dir);
    BOOST_CHECK_CLOSE_FRACTION(u, -sqrt(2)/2, 0.001);
    BOOST_CHECK_CLOSE_FRACTION(v, -sqrt(2)/2, 0.001);
}


BOOST_AUTO_TEST_CASE(test_uv_spd_dir)
{
    // make sure it works to go back and forth from 0 components.
    float u = 0;
    float v = 0;
    float dir = 45;
    float spd = 1;
    derive_spd_dir_from_uv(spd, dir, u, v);
    BOOST_CHECK(isnanf(dir));
    BOOST_CHECK_EQUAL(spd, 0);
    u = 1;
    v = 1;
    derive_uv_from_spd_dir(u, v, spd, dir);
    BOOST_CHECK_EQUAL(u, 0);
    BOOST_CHECK_EQUAL(v, 0);
}


BOOST_AUTO_TEST_CASE(test_wind2d_flipped)
{
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(LogConfig()));

    Wind2D wind;
    setup_wind2d(wind, true);

    BOOST_CHECK(wind.getDSMId() == 1);
    BOOST_CHECK(wind.getSensorId() == 4);

    // the orienter should flip upside down 2D components
    float u = 1;
    float v = 2;
    wind.getOrienter().applyOrientation2D(&u, &v);
    BOOST_CHECK(u == 1);
    BOOST_CHECK(v == -2);

    std::list<const Sample*> results;
    SampleT<char>* samp = 
        getSample("\x02""A,324,001.28,M,+008.10,00,\x03""10\r");
    // SampleChar samp("\x02""A,324,001.28,M,+008.10,00,\x03""10\r");
    BOOST_CHECK(samp->getDataLength() == 32);
    // SampleT<char> samp;
    // samp << "\x02""A,324,001.28,M,+008.10,00,\x03""10\r";
    BOOST_CHECK(wind.process(samp, results));
    samp->freeReference();
    BOOST_CHECK(results.size() == 1);

    const Sample* csamp = results.front();

    // Given raw direction of 324, flipped vector direction would be 36,
    // then wind from direction is 36+180 = 216.
    BOOST_CHECK_CLOSE_FRACTION(csamp->getDataValue(0), 216, 0.1);
    csamp->freeReference();
    SamplePools::deleteInstance();
}

BOOST_AUTO_TEST_CASE(test_wind2d_flipped_offset)
{
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(LogConfig()));

    Wind2D wind;
    setup_wind2d(wind, true, 45);

    std::list<const Sample*> results;
    SampleT<char>* samp = 
        getSample("\x02""A,324,001.28,M,+008.10,00,\x03""10\r");

    BOOST_CHECK(wind.process(samp, results));
    samp->freeReference();
    BOOST_CHECK(results.size() == 1);
    const Sample* csamp = results.front();

    // Given raw direction of 324, flipped vector direction would be 36, then
    // wind from direction is 36+180 = 216, plus the offset of 45 is
    // 261.
    BOOST_CHECK_CLOSE_FRACTION(csamp->getDataValue(0), 261, 0.1);
    csamp->freeReference();
    SamplePools::deleteInstance();
}
