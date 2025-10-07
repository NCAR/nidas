#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <functional>


#include <nidas/dynld/isff/Wind3D.h>
#include <nidas/dynld/isff/CSI_IRGA_Sonic.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/Project.h>
#include <nidas/core/Variable.h>
#include <nidas/core/VariableIndex.h>
#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/UTime.h>
#include <memory>
#include <nidas/util/util.h>

using namespace nidas::util;
using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::isff;

using nidas::util::derive_spd_dir_from_uv;
using nidas::util::derive_uv_from_spd_dir;


const std::string log_config = "level=error";


namespace testing {

struct Wind3D_test {
public:
    Wind3D_test(Wind3D& wind):
        _diagIndex(wind._diagIndex),
        _ldiagIndex(wind._ldiagIndex),
        _spdIndex(wind._spdIndex),
        _dirIndex(wind._dirIndex),
        _spikeIndex(wind._spikeIndex),
        _noutVals(wind._noutVals),
        _numParsed(wind._numParsed),
        _shadowFactor(wind._shadowFactor)
    {}

    int _diagIndex;
    int _ldiagIndex;
    int _spdIndex;
    int _dirIndex;
    nidas::core::VariableIndex _spikeIndex;
    unsigned int _noutVals;
    unsigned int _numParsed;
    double _shadowFactor;
};

}


using testing::Wind3D_test;


// Typical XML for a CSI_IRGA sensor.
const char* irga_xml = R"XML(
    <serialSensor class="isff.CSI_IRGA_Sonic" ID="CSAT3_IRGA_BIN"
        baud="115200" parity="none" databits="8" stopbits="1"
        devicename="/dev/ttyDSM1" id="4">
        <parameter name="bandwidth" type="float" value="5"/>
        <parameter type="float" name="shadowFactor" value="0"/>
        <parameter type="bool" name="despike" value="${CSAT3_DESPIKE}"/>
        <sample id="1" rate="20">
            <variable name="u" units="m/s" longname="Wind U component, CSAT3"/>
            <variable name="v" units="m/s" longname="Wind V component, CSAT3"/>
            <variable name="w" units="m/s" longname="Wind W component, CSAT3"/>
            <variable name="tc" units="degC" longname="Virtual air temperature from speed of sound, CSAT3"/>
            <variable name="diagbits" units="" longname="CSAT3 diagnostic sum, 1=low sig,2=high sig,4=no lock,8=path diff,16=skipped samp"/>
            <variable name="co2" units="mg/m^3" longname="CO2 density from CSI IRGA"/>
            <variable name="h2o" units="g/m^3" longname="Water vapor density from CSI IRGA" minValue="-1.00" maxValue="10.0"/>
            <variable name="irgadiag" units="" longname="CSI IRGA diagnostic" plotrange="$DIAG_RANGE"/>
            <variable name="Tirga" units="degC" longname="CSI IRGA temperature"/>
            <variable name="Pirga" units="kPa" longname="CSI IRGA pressure"/>
            <variable name="SSco2" units="" longname="CSI IRGA CO2 signal strength"/>
            <variable name="SSh2o" units="" longname="CSI IRGA H2O signal strength"/>
            <variable name="dPirga" units="mb" longname="CSI IRGA differential pressure"/>
            <!-- derived variables ldiag, spd, dir should be at the end of the sample -->
            <variable name="ldiag" units="" longname="CSAT3 logical diagnostic, 0=OK, 1=(diagbits!=0)"/>
            <variable name="spd" units="m/s" longname="CSAT3 horizontal wind speed"/>
            <variable name="dir" units="deg" longname="CSAT3 wind direction"/>
        </sample>
        <message separator="\x55\xaa" position="end" length="58"/>
    </serialSensor>
)XML";


void
setup_wind3d(Wind3D& wind, const char* sonic_xml, bool despike=false)
{
    static char env_despike[128];
    sprintf(env_despike, "CSAT3_DESPIKE=%s", despike ? "true" : "false");
    putenv(env_despike);

    wind.setDSMId(1);

    std::string xml = Project::getInstance()->expandString(sonic_xml);
    std::unique_ptr<xercesc::DOMDocument> doc(XMLParser::ParseString(xml));
    wind.fromDOMElement(doc->getDocumentElement());
    wind.validate();
    wind.init();

    doc.reset();
    XMLImplementation::terminate();
}


BOOST_AUTO_TEST_CASE(test_wind3d_no_despike)
{
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(log_config));

    // the output variables should not change if despiking is on or off
    for (auto despike : {false, true})
    {
        CSI_IRGA_Sonic wind;
        setup_wind3d(wind, irga_xml, despike);

        BOOST_TEST(wind.getDSMId() == 1);
        BOOST_TEST(wind.getSensorId() == 4);
        BOOST_TEST(wind.getDespike() == despike);

        Wind3D_test pwind(wind);
        BOOST_TEST(pwind._spdIndex == 14);
        BOOST_TEST(pwind._dirIndex == 15);
        BOOST_TEST(pwind._diagIndex == 4);
        BOOST_TEST(pwind._spikeIndex.valid() == false);
        BOOST_TEST(pwind._ldiagIndex == 13);
        // 13 are parsed, 3 derived (ldiag, spd, dir)
        BOOST_TEST(pwind._numParsed == 13);
        BOOST_TEST(pwind._noutVals == 16);
        BOOST_TEST(pwind._shadowFactor == 0.0);
    }
}


// Typical XML for a CSI_IRGA sensor.
const char* irga_xml_with_flags = R"XML(
    <serialSensor class="isff.CSI_IRGA_Sonic" ID="CSAT3_IRGA_BIN"
        baud="115200" parity="none" databits="8" stopbits="1"
        devicename="/dev/ttyDSM1" id="4">
        <parameter name="bandwidth" type="float" value="5"/>
        <parameter type="float" name="shadowFactor" value="0"/>
        <parameter type="bool" name="despike" value="${CSAT3_DESPIKE}"/>
        <sample id="1" rate="20">
            <variable name="u" units="m/s" longname="Wind U component, CSAT3"/>
            <variable name="v" units="m/s" longname="Wind V component, CSAT3"/>
            <variable name="w" units="m/s" longname="Wind W component, CSAT3"/>
            <variable name="tc" units="degC" longname="Virtual air temperature from speed of sound, CSAT3"/>
            <variable name="diagbits" units="" longname="CSAT3 diagnostic sum, 1=low sig,2=high sig,4=no lock,8=path diff,16=skipped samp"/>
            <variable name="co2" units="mg/m^3" longname="CO2 density from CSI IRGA"/>
            <variable name="h2o" units="g/m^3" longname="Water vapor density from CSI IRGA" minValue="-1.00" maxValue="10.0"/>
            <variable name="irgadiag" units="" longname="CSI IRGA diagnostic" plotrange="$DIAG_RANGE"/>
            <variable name="Tirga" units="degC" longname="CSI IRGA temperature"/>
            <variable name="Pirga" units="kPa" longname="CSI IRGA pressure"/>
            <variable name="SSco2" units="" longname="CSI IRGA CO2 signal strength"/>
            <variable name="SSh2o" units="" longname="CSI IRGA H2O signal strength"/>
            <variable name="dPirga" units="mb" longname="CSI IRGA differential pressure"/>
            <!-- derived variables should be at the end of the sample -->
            <variable name="uflag" units="1" longname="Spike detection flag for U component"/>
            <variable name="vflag" units="1" longname="Spike detection flag for V component"/>
            <variable name="wflag" units="1" longname="Spike detection flag for W component"/>
            <variable name="tcflag" units="1" longname="Spike detection flag for Tc"/>
            <variable name="ldiag" units="" longname="CSAT3 logical diagnostic, 0=OK, 1=(diagbits!=0)"/>
            <variable name="spd" units="m/s" longname="CSAT3 horizontal wind speed"/>
            <variable name="dir" units="deg" longname="CSAT3 wind direction"/>
        </sample>
        <message separator="\x55\xaa" position="end" length="58"/>
    </serialSensor>
)XML";


BOOST_AUTO_TEST_CASE(test_wind3d_with_flags)
{
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(log_config));

    // the output variables should not change if despiking is on or off
    for (auto despike : {false, true})
    {
        CSI_IRGA_Sonic wind;
        setup_wind3d(wind, irga_xml_with_flags, despike);

        BOOST_TEST(wind.getDSMId() == 1);
        BOOST_TEST(wind.getSensorId() == 4);
        BOOST_TEST(wind.getDespike() == despike);

        Wind3D_test pwind(wind);
        BOOST_TEST(pwind._spdIndex == 18);
        BOOST_TEST(pwind._dirIndex == 19);
        BOOST_TEST(pwind._diagIndex == 4);
        BOOST_TEST(pwind._spikeIndex.valid() == true);
        BOOST_TEST(pwind._spikeIndex.index() == 13);
        BOOST_TEST(pwind._spikeIndex.variable()->getName() == "uflag");
        BOOST_TEST(pwind._ldiagIndex == 17);
        // 13 are parsed, 7 derived (ldiag, spd, dir)
        BOOST_TEST(pwind._numParsed == 13);
        BOOST_TEST(pwind._noutVals == 20);
        BOOST_TEST(pwind._shadowFactor == 0.0);
    }
}


// Typical XML for a CSI_IRGA sensor.
const char* irga_xml_with_flags_at_end = R"XML(
    <serialSensor class="isff.CSI_IRGA_Sonic" ID="CSAT3_IRGA_BIN"
        baud="115200" parity="none" databits="8" stopbits="1"
        devicename="/dev/ttyDSM1" id="4">
        <parameter name="bandwidth" type="float" value="5"/>
        <parameter type="float" name="shadowFactor" value="0"/>
        <parameter type="bool" name="despike" value="${CSAT3_DESPIKE}"/>
        <sample id="1" rate="20">
            <variable name="u" units="m/s" longname="Wind U component, CSAT3"/>
            <variable name="v" units="m/s" longname="Wind V component, CSAT3"/>
            <variable name="w" units="m/s" longname="Wind W component, CSAT3"/>
            <variable name="tc" units="degC" longname="Virtual air temperature from speed of sound, CSAT3"/>
            <variable name="diagbits" units="" longname="CSAT3 diagnostic sum, 1=low sig,2=high sig,4=no lock,8=path diff,16=skipped samp"/>
            <variable name="co2" units="mg/m^3" longname="CO2 density from CSI IRGA"/>
            <variable name="h2o" units="g/m^3" longname="Water vapor density from CSI IRGA" minValue="-1.00" maxValue="10.0"/>
            <variable name="irgadiag" units="" longname="CSI IRGA diagnostic" plotrange="$DIAG_RANGE"/>
            <variable name="Tirga" units="degC" longname="CSI IRGA temperature"/>
            <variable name="Pirga" units="kPa" longname="CSI IRGA pressure"/>
            <variable name="SSco2" units="" longname="CSI IRGA CO2 signal strength"/>
            <variable name="SSh2o" units="" longname="CSI IRGA H2O signal strength"/>
            <variable name="dPirga" units="mb" longname="CSI IRGA differential pressure"/>
            <!-- derived variables ldiag, spd, dir should be at the end of the sample -->
            <variable name="ldiag" units="" longname="CSAT3 logical diagnostic, 0=OK, 1=(diagbits!=0)"/>
            <variable name="spd" units="m/s" longname="CSAT3 horizontal wind speed"/>
            <variable name="dir" units="deg" longname="CSAT3 wind direction"/>
            <variable name="uflag" units="1" longname="Spike detection flag for U component"/>
            <variable name="vflag" units="1" longname="Spike detection flag for V component"/>
            <variable name="wflag" units="1" longname="Spike detection flag for W component"/>
            <variable name="tcflag" units="1" longname="Spike detection flag for Tc"/>
        </sample>
        <message separator="\x55\xaa" position="end" length="58"/>
    </serialSensor>
)XML";


BOOST_AUTO_TEST_CASE(test_wind3d_with_flags_at_end)
{
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(log_config));

    // the output variables should not change if despiking is on or off
    for (auto despike : {false, true})
    {
        CSI_IRGA_Sonic wind;
        setup_wind3d(wind, irga_xml_with_flags_at_end, despike);

        BOOST_TEST(wind.getDSMId() == 1);
        BOOST_TEST(wind.getSensorId() == 4);
        BOOST_TEST(wind.getDespike() == despike);

        Wind3D_test pwind(wind);
        BOOST_TEST(pwind._spdIndex == 14);
        BOOST_TEST(pwind._dirIndex == 15);
        BOOST_TEST(pwind._diagIndex == 4);
        BOOST_TEST(pwind._spikeIndex.valid() == true);
        BOOST_TEST(pwind._spikeIndex.index() == 16);
        BOOST_TEST(pwind._spikeIndex.variable()->getName() == "uflag");
        BOOST_TEST(pwind._ldiagIndex == 13);
        // 13 are parsed, 7 derived (ldiag, spd, dir)
        BOOST_TEST(pwind._numParsed == 13);
        BOOST_TEST(pwind._noutVals == 20);
        BOOST_TEST(pwind._shadowFactor == 0.0);
    }
}


const char* irga_xml_wrong_order = R"XML(
    <serialSensor class="isff.CSI_IRGA_Sonic" ID="CSAT3_IRGA_BIN"
        baud="115200" parity="none" databits="8" stopbits="1"
        devicename="/dev/ttyDSM1" id="4">
        <parameter name="bandwidth" type="float" value="5"/>
        <parameter type="float" name="shadowFactor" value="0"/>
        <parameter type="bool" name="despike" value="${CSAT3_DESPIKE}"/>
        <sample id="1" rate="20">
            <variable name="u" units="m/s" longname="Wind U component, CSAT3"/>
            <variable name="v" units="m/s" longname="Wind V component, CSAT3"/>
            <variable name="w" units="m/s" longname="Wind W component, CSAT3"/>
            <variable name="tc" units="degC" longname="Virtual air temperature from speed of sound, CSAT3"/>
            <variable name="diagbits" units="" longname="CSAT3 diagnostic sum, 1=low sig,2=high sig,4=no lock,8=path diff,16=skipped samp"/>
            <variable name="ldiag" units="" longname="CSAT3 logical diagnostic, 0=OK, 1=(diagbits!=0)"/>
            <variable name="co2" units="mg/m^3" longname="CO2 density from CSI IRGA"/>
            <variable name="h2o" units="g/m^3" longname="Water vapor density from CSI IRGA" minValue="-1.00" maxValue="10.0"/>
            <variable name="irgadiag" units="" longname="CSI IRGA diagnostic" plotrange="$DIAG_RANGE"/>
            <variable name="Tirga" units="degC" longname="CSI IRGA temperature"/>
            <variable name="Pirga" units="kPa" longname="CSI IRGA pressure"/>
            <variable name="SSco2" units="" longname="CSI IRGA CO2 signal strength"/>
            <variable name="SSh2o" units="" longname="CSI IRGA H2O signal strength"/>
            <variable name="dPirga" units="mb" longname="CSI IRGA differential pressure"/>
            <!-- derived variables ldiag, spd, dir should be at the end of the sample -->
            <variable name="spd" units="m/s" longname="CSAT3 horizontal wind speed"/>
            <variable name="dir" units="deg" longname="CSAT3 wind direction"/>
            <variable name="uflag" units="1" longname="Spike detection flag for U component"/>
            <variable name="vflag" units="1" longname="Spike detection flag for V component"/>
            <variable name="wflag" units="1" longname="Spike detection flag for W component"/>
            <variable name="tcflag" units="1" longname="Spike detection flag for Tc"/>
        </sample>
        <message separator="\x55\xaa" position="end" length="58"/>
    </serialSensor>
)XML";


BOOST_AUTO_TEST_CASE(test_wind3d_wrong_order)
{
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(log_config));

    try {
        CSI_IRGA_Sonic wind;
        setup_wind3d(wind, irga_xml_wrong_order, true);
        BOOST_FAIL("Expected InvalidParameterException");
    } catch (const InvalidParameterException& e) {
        std::string msg = e.what();
        BOOST_TEST(msg.find("derived variables must be at the end") != std::string::npos);
    } catch (...) {
        BOOST_FAIL("Expected InvalidParameterException");
    }
}


BOOST_AUTO_TEST_CASE(test_adaptive_despiker)
{
    AdaptiveDespiker despiker;

    BOOST_TEST(despiker.numPoints() == 0);
    double dmean{0}, dvar{0}, dcorr{0};

    // Create a vector of values with known mean and standard deviation.
    int nvalues = 102;
    std::vector<float> values(nvalues);
    float mean = 10.0;
    float stddev = 2.0;
    for (int i = 0; i < nvalues; ++i)
        values[i] = mean + (2 * (i % 2) - 1) * stddev;

    dsm_time_t t = UTime::parse("2023-01-01T00:00:00Z").toUsecs();
    bool spike { false };

    // Feed the values to the despiker, and check that none are flagged as spikes.
    for (const auto& value : values) {
        despiker.despike(t, value, &spike);
        BOOST_TEST(spike == false);
        t += 1000000;  // Increment time by 1 second.
    }
    despiker.getStatistics(&dmean, &dvar, &dcorr);
    BOOST_TEST(despiker.numPoints() == (size_t)nvalues);
    BOOST_CHECK_CLOSE(dmean, mean, 0.01);
    BOOST_CHECK_CLOSE_FRACTION(dvar, stddev*stddev, 0.01);

    // Do it again, but make sure a spike is still not a spike if the minimum
    // number of samples is not reached yet.
    despiker.reset();
    float vspike = mean + 10 * stddev;
    float saved = values[99];
    values[99] = vspike;
    for (const auto& value : values) {
        despiker.despike(t, value, &spike);
        BOOST_TEST(spike == false);
        t += 1000000;  // Increment time by 1 second.
    }

    // Do it again, but the spike comes after the minimum reached.
    despiker.reset();
    values[99] = saved;
    for (const auto& value : values) {
        float result = despiker.despike(t, value, &spike);
        BOOST_TEST(result == value);
        BOOST_TEST(spike == false);
        t += 1000000;  // Increment time by 1 second.
    }
    float result = despiker.despike(t, vspike, &spike);
    BOOST_TEST_MESSAGE("despike(" << vspike << "): " << result);
    BOOST_TEST(result != vspike);
    BOOST_TEST(spike == true);
    // because the last point was a spike, it is not included in the count.
    BOOST_TEST(despiker.numPoints() == nvalues);
    despiker.getStatistics(&dmean, &dvar, &dcorr);
    // should still get the same statistics as before the minimum samples was
    // reached.
    BOOST_CHECK_CLOSE_FRACTION(dmean, mean, 0.01);
    BOOST_CHECK_CLOSE_FRACTION(dvar, stddev*stddev, 0.01);

    // The same value after too much time causes a reset.
    t += AdaptiveDespiker::DATA_GAP_USEC;
    result = despiker.despike(t, vspike, &spike);
    BOOST_TEST(result == vspike);
    BOOST_TEST(spike == false);
    BOOST_TEST(despiker.numPoints() == 1);
}
