#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

namespace tt = boost::test_tools;

#include <functional>
#include <iomanip>


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


class LogFixture
{
    void setup() {
        Logger* logger = Logger::createInstance(&std::cerr);
        logger->setScheme(LogScheme().addConfig(log_config));
    }

    void teardown() {
        Logger::destroyInstance();
    }
};

BOOST_TEST_GLOBAL_FIXTURE(LogFixture);


// As near as I can tell from the documentation, BOOST_TEST() is supposed to
// allow absolute tolerance comparisons, but that behavior must have changed
// or been broken somewhere along the way, since in my testing I can only get
// relative differences.  So create a macro to force it until something better
// comes along.

#define TEST_CLOSE(got, expected, tolerance) \
    BOOST_CHECK_SMALL(std::abs((got) - (expected)), (tolerance))


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
static const char* irga_xml = R"XML(
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
            <variable name="co2" units="mg/m^3" longname="CO2 density from CSI IRGA">
                <linear units="g/m^3" slope="0.001" intercept="0.0"/>
            </variable>
            <variable name="h2o" units="g/m^3" longname="Water vapor density from CSI IRGA" minValue="-1.00" maxValue="10.0"/>
            <variable name="irgadiag" units="" longname="CSI IRGA diagnostic" plotrange="$DIAG_RANGE"/>
            <variable name="Tirga" units="degC" longname="CSI IRGA temperature"/>
            <variable name="Pirga" units="kPa" longname="CSI IRGA pressure">
                <linear units="mb" slope="10" intercept="0.0"/>
            </variable>
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
    doc.reset();
    XMLImplementation::terminate();

    // A Site enables variable conversions by default, but since there is no
    // Site associated with this standalone sensor, and for some reason the
    // sensor default is false, the variable conversions must be enabled
    // explicitly. Go figure.
    wind.setApplyVariableConversions(true);

    // these may throw, so call them after the XML is freed
    wind.validate();
    wind.init();
}


BOOST_AUTO_TEST_CASE(test_wind3d_no_despike)
{
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
static const char* irga_xml_with_flags = R"XML(
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
            <variable name="co2" units="mg/m^3" longname="CO2 density from CSI IRGA">
                <linear units="g/m^3" slope="0.001" intercept="0.0"/>
            </variable>
            <variable name="h2o" units="g/m^3" longname="Water vapor density from CSI IRGA" minValue="-1.00" maxValue="10.0"/>
            <variable name="irgadiag" units="" longname="CSI IRGA diagnostic" plotrange="$DIAG_RANGE"/>
            <variable name="Tirga" units="degC" longname="CSI IRGA temperature"/>
            <variable name="Pirga" units="kPa" longname="CSI IRGA pressure">
                <linear units="mb" slope="10" intercept="0.0"/>
            </variable>
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
static const char* irga_xml_with_flags_at_end = R"XML(
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
            <variable name="co2" units="mg/m^3" longname="CO2 density from CSI IRGA">
                <linear units="g/m^3" slope="0.001" intercept="0.0"/>
            </variable>
            <variable name="h2o" units="g/m^3" longname="Water vapor density from CSI IRGA" minValue="-1.00" maxValue="10.0"/>
            <variable name="irgadiag" units="" longname="CSI IRGA diagnostic" plotrange="$DIAG_RANGE"/>
            <variable name="Tirga" units="degC" longname="CSI IRGA temperature"/>
            <variable name="Pirga" units="kPa" longname="CSI IRGA pressure">
                <linear units="mb" slope="10" intercept="0.0"/>
            </variable>
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


static const char* irga_xml_wrong_order = R"XML(
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
            <variable name="co2" units="mg/m^3" longname="CO2 density from CSI IRGA">
                <linear units="g/m^3" slope="0.001" intercept="0.0"/>
            </variable>
            <variable name="h2o" units="g/m^3" longname="Water vapor density from CSI IRGA" minValue="-1.00" maxValue="10.0"/>
            <variable name="irgadiag" units="" longname="CSI IRGA diagnostic" plotrange="$DIAG_RANGE"/>
            <variable name="Tirga" units="degC" longname="CSI IRGA temperature"/>
            <variable name="Pirga" units="kPa" longname="CSI IRGA pressure">
                <linear units="mb" slope="10" intercept="0.0"/>
            </variable>
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


static const char* irga_xml_few_flags = R"XML(
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
            <variable name="co2" units="mg/m^3" longname="CO2 density from CSI IRGA">
                <linear units="g/m^3" slope="0.001" intercept="0.0"/>
            </variable>
            <variable name="h2o" units="g/m^3" longname="Water vapor density from CSI IRGA" minValue="-1.00" maxValue="10.0"/>
            <variable name="irgadiag" units="" longname="CSI IRGA diagnostic" plotrange="$DIAG_RANGE"/>
            <variable name="Tirga" units="degC" longname="CSI IRGA temperature"/>
            <variable name="Pirga" units="kPa" longname="CSI IRGA pressure">
                <linear units="mb" slope="10" intercept="0.0"/>
            </variable>
            <variable name="SSco2" units="" longname="CSI IRGA CO2 signal strength"/>
            <variable name="SSh2o" units="" longname="CSI IRGA H2O signal strength"/>
            <variable name="dPirga" units="mb" longname="CSI IRGA differential pressure"/>
            <!-- derived variables ldiag, spd, dir should be at the end of the sample -->
            <variable name="uflag" units="1" longname="Spike detection flag for U component"/>
            <variable name="vflag" units="1" longname="Spike detection flag for V component"/>
            <variable name="wflag" units="1" longname="Spike detection flag for W component"/>
            <variable name="ldiag" units="" longname="CSAT3 logical diagnostic, 0=OK, 1=(diagbits!=0)"/>
            <variable name="spd" units="m/s" longname="CSAT3 horizontal wind speed"/>
            <variable name="dir" units="deg" longname="CSAT3 wind direction"/>
            <variable name="tcflag" units="1" longname="Spike detection flag for Tc"/>
        </sample>
        <message separator="\x55\xaa" position="end" length="58"/>
    </serialSensor>
)XML";


BOOST_AUTO_TEST_CASE(test_wind3d_few_flags)
{
    try {
        CSI_IRGA_Sonic wind;
        setup_wind3d(wind, irga_xml_few_flags, true);
        BOOST_FAIL("Expected InvalidParameterException");
    } catch (const InvalidParameterException& e) {
        std::string msg = e.what();
        std::string xmsg = "spike flag variables must be uflag,vflag,wflag,tcflag";
        BOOST_TEST(msg.find(xmsg) != std::string::npos);
    } catch (...) {
        BOOST_FAIL("Expected InvalidParameterException");
    }
}


class BinaryMessage : public std::vector<char> {
public:
    BinaryMessage(const char* hex_data) {
        std::istringstream ibuf(hex_data);
        std::string hexstr;
        while (ibuf >> hexstr) {
            char val = static_cast<char>(strtol(hexstr.c_str(), nullptr, 16));
            this->push_back(val);
        }
    }
};


struct SonicSample {
    std::string raw_timestamp;
    int dsm_id;
    int sensor_id;
    BinaryMessage raw_data;
    std::string processed_timestamp;
    int processed_id;
    std::vector<float> processed_data;

    SampleT<char>* getSample() const
    {
        SampleT<char>* samp = nidas::core::getSample<char>(raw_data.size());
        std::memcpy(samp->getDataPtr(), raw_data.data(), raw_data.size());
        return samp;
    }
};


// env CSAT3_SHADOW_FACTOR=0.0 data_dump --precision 12 --nodeltat -H --iso -i 120,1030-1031 --xml .../m2hats1.xml -p t0t_20230804_120000.dat
std::vector<SonicSample> test_samples {
    SonicSample{
"2023-08-04T12:00:00.0054", 1, 4, R"(b4 dc fe bf 1b 54 25 be 39 22 12 3e 4c 0e 5d 41 00 00 00 00 9e ea 1f 44 25 19 84 40 00 00 00 00 40 e1 4d 41 26 92 a7 42 92 7c 76 3f 19 c5 75 3f 40 4d 21 44 9a f6 dc 00 2d 84 55 aa)",
"2023-08-04T11:59:59.2054", 5, {
    -1.99111032486, -0.161453649402, 0.142708674073, 13.815990448, 0, 0.639665901661, 4.12806940079, 0, 12.8674926758, 837.854492188, 0.962838292122, 0.960038721561, 645.20703125, 0, 1.99764549732, 85.3641815186 }
    },
    SonicSample{
"2023-08-04T12:00:00.0214", 1, 4, R"(22 b0 07 c0 74 36 d6 bd 80 6c 15 3e 35 ba 5d 41 00 00 00 00 e4 f5 1f 44 01 47 84 40 00 00 00 00 40 e1 4d 41 26 92 a7 42 4a 7a 76 3f dd c6 75 3f 1e 5a 21 44 9b f6 dc 00 25 5c 55 aa)",
"2023-08-04T11:59:59.2214", 5, {
    -2.12012529373, -0.104596048594, 0.145921707153, 13.857960701, 0, 0.639842092991, 4.13366746902, 0, 12.8674926758, 837.854492188, 0.962803483009, 0.960065662861, 645.408081055, 0, 2.12270379066, 87.1756134033 }
    },
    SonicSample{
"2023-08-04T12:00:00.1180", 1, 4, R"(b3 9f 0a c0 18 90 83 3e 8b dd 7b 3e 5a ed 5d 41 00 00 00 00 83 ea 1f 44 37 33 84 40 00 00 00 00 40 e1 4d 41 26 92 a7 42 12 7b 76 3f ed c7 75 3f 1e 4f 21 44 9c f6 dc 00 46 d4 55 aa)",
"2023-08-04T11:59:59.3180", 5, {
    -2.16599726677, 0.256958723068, 0.245962306857, 13.8704471588, 0, 0.639664292336, 4.13125181198, 0, 12.8674926758, 837.854492188, 0.962815403938, 0.960081875324, 645.236206055, 0, 2.18118596077, 96.7655487061 }
    }
};


const int IXU = 0;
const int IXV = 1;
const int IXW = 2;
const int IXTC = 3;
const int IXSPD = 14;
const int IXDIR = 15;
const int IXUFLAG = 16;
const int IXVFLAG = 17;
const int IXWFLAG = 18;
const int IXTCFLAG = 19;



static void
process_wind3d_samples(CSI_IRGA_Sonic& wind,
    const std::vector<SonicSample>& samples)
{
    int nsample = 0;
    for (const auto& sample : samples)
    {
        ++nsample;

        SampleT<char>* samp = sample.getSample();
        samp->setDSMId(sample.dsm_id);
        samp->setSpSId(sample.sensor_id);
        samp->setTimeTag(UTime::parse(sample.raw_timestamp).toUsecs());

        std::list<const Sample*> results;
        wind.process(samp, results);
        samp->freeReference();
        BOOST_REQUIRE(results.size() == 1);
        dsm_time_t proc_time = UTime::parse(sample.processed_timestamp).toUsecs();
        const Sample* psamp = results.front();

        BOOST_TEST(psamp->getTimeTag() == proc_time);
        BOOST_TEST(psamp->getDSMId() == sample.dsm_id);
        BOOST_TEST(psamp->getSpSId() == sample.processed_id);

        const std::vector<float>& xpd = sample.processed_data;
        bool despiked = false;
        if (xpd.size() == IXTCFLAG + 1) {
            despiked = xpd[IXUFLAG] || xpd[IXVFLAG] || xpd[IXWFLAG] || xpd[IXTCFLAG];
        }

        BOOST_TEST(psamp->getDataLength() == sample.processed_data.size());
        for (size_t i = 0; i < sample.processed_data.size(); ++i) {
            float tolerance = 0.001;
            if (despiked && i <= IXTC && xpd[i+IXUFLAG])
                tolerance = 0.1;
            if (despiked && (i == IXSPD || i == IXDIR))
                tolerance = 1.0;

            float fgot = (float)psamp->getDataValue(i);
            DLOG(("") << "nsample=" << nsample << ";i=" << i
                      << " got=" << fgot << " expected=" << xpd[i]
                      << ", abs(diff)=" << abs(fgot - xpd[i])
                      << ", tolerance=" << tolerance);
            TEST_CLOSE((float)psamp->getDataValue(i), xpd[i], tolerance);
        }
        psamp->freeReference();
    }
    SamplePools::deleteInstance();
}


BOOST_AUTO_TEST_CASE(test_wind3d_process)
{
    CSI_IRGA_Sonic wind;

    // first try the standard processing, without despiking and without flags
    setup_wind3d(wind, irga_xml, false);
    process_wind3d_samples(wind, test_samples);
}


BOOST_AUTO_TEST_CASE(test_wind3d_process_flatlines)
{
    CSI_IRGA_Sonic wind;
    setup_wind3d(wind, irga_xml_with_flags_at_end, true);

    std::vector<SonicSample> samples_with_flags = test_samples;
    for (auto& sample : samples_with_flags) {
        // add 4 flag values (no spikes detected, so all zero)
        sample.processed_data.insert(
            sample.processed_data.end(), {0.0, 0.0, 0.0, 0.0});
    }

    // make sure flatlines do not cause problems
    std::vector<SonicSample> flatlines(500, samples_with_flags[0]);
    process_wind3d_samples(wind, flatlines);
}


BOOST_AUTO_TEST_CASE(test_wind3d_process_spikes)
{
    CSI_IRGA_Sonic wind;
    setup_wind3d(wind, irga_xml_with_flags_at_end, true);

    std::vector<SonicSample> samples_with_flags = test_samples;
    for (auto& sample : samples_with_flags) {
        // add 4 flag values (no spikes detected, so all zero)
        sample.processed_data.insert(
            sample.processed_data.end(), {0.0, 0.0, 0.0, 0.0});
    }

    std::vector<SonicSample> spike_samples;
    for (unsigned int i = 0; i < 500; ++i)
    {
        SonicSample ss = samples_with_flags[i % samples_with_flags.size()];
        if (i % 75 == 0 && i > 100)
        {
            CSI_IRGA_Fields fields;
            wind.unpackBinary(ss.raw_data.data(),
                              ss.raw_data.data() + ss.raw_data.size(),
                              fields);
            // introduce a spike
            fields.v += 10.0;
            ss.processed_data[17] = 1.0; // set vflag
            // repack the binary data with the spike
            wind.packBinary(fields, ss.raw_data);
            // leave the expected processed data value, since the despiker
            // should replace it with something close.
        }
        spike_samples.push_back(ss);
    }
    process_wind3d_samples(wind, spike_samples);
}


BOOST_AUTO_TEST_CASE(test_wind3d_process_with_despike_and_flags)
{
    CSI_IRGA_Sonic wind;

    // now try with despiking and flags
    setup_wind3d(wind, irga_xml_with_flags_at_end, true);

    std::vector<SonicSample> samples_with_flags = test_samples;
    for (auto& sample : samples_with_flags) {
        // add 4 flag values (no spikes detected, so all zero)
        sample.processed_data.insert(
            sample.processed_data.end(), {0.0, 0.0, 0.0, 0.0});
    }

    process_wind3d_samples(wind, samples_with_flags);
}


std::string to_hex(const char* buf, size_t len)
{
    std::ostringstream os;
    for (size_t i = 0; i < len; ++i) {
        os << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(static_cast<unsigned char>(buf[i])) << " ";
    }
    return os.str();
}


BOOST_AUTO_TEST_CASE(test_wind3d_unpacker)
{
    CSI_IRGA_Sonic wind;
    setup_wind3d(wind, irga_xml, false);

    for (const auto& sample : test_samples) {
        SampleT<char>* samp = sample.getSample();

        CSI_IRGA_Fields values;
        const char* buf = samp->getDataPtr();
        const char* eob = samp->getDataPtr() + samp->getDataLength();
        int nvals = wind.unpackBinary(buf, eob, values);

        // the binary samples have 14 fields, all but Tdetector, even though
        // the test samples only parsed 13, so check that the last is NaN.
        BOOST_TEST(nvals == 14);
        TEST_CLOSE(values.u, sample.processed_data[0], 0.001f);
        TEST_CLOSE(values.v, sample.processed_data[1], 0.001f);
        TEST_CLOSE(values.w, sample.processed_data[2], 0.001f);
        TEST_CLOSE(values.tc, sample.processed_data[3], 0.001f);
        TEST_CLOSE(values.dPirga, sample.processed_data[12], 0.001f);
        BOOST_TEST(!isnanf(values.Tsource));
        BOOST_TEST(isnanf(values.Tdetector));

        // and it should work to pack it back to binary
        std::vector<char> outbuf;
        wind.packBinary(values, outbuf);
        BOOST_TEST(outbuf.size() == samp->getDataLength());
        DLOG(("") << "expected: "
                  << to_hex(samp->getDataPtr(), samp->getDataLength()));
        DLOG(("") << "     got: "
                  << to_hex(outbuf.data(), outbuf.size()));
        BOOST_TEST(std::memcmp(outbuf.data(), samp->getDataPtr(),
                   outbuf.size()) == 0);

        samp->freeReference();
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
    TEST_CLOSE(dmean, mean, 0.01);
    TEST_CLOSE(dvar, stddev*stddev, 0.01);

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
    TEST_CLOSE(dmean, mean, 0.01);
    TEST_CLOSE(dvar, stddev*stddev, 0.01);

    // The same value after too much time causes a reset.
    t += AdaptiveDespiker::DATA_GAP_USEC;
    result = despiker.despike(t, vspike, &spike);
    BOOST_TEST(result == vspike);
    BOOST_TEST(spike == false);
    BOOST_TEST(despiker.numPoints() == 1);
}
