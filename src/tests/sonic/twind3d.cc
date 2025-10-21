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
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(log_config));

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


struct SonicSample {
    std::string raw_timestamp;
    int dsm_id;
    int sensor_id;
    const char* raw_data;
    std::string processed_timestamp;
    int processed_id;
    std::vector<float> processed_data;
};


// env CSAT3_SHADOW_FACTOR=0.0 data_dump --nodeltat -H --iso -i 120,1030-1031 --xml /home/granger/code/isfs/projects/M2HATS/ISFS/config/m2hats1.xml -p t0t_20230804_120000.dat
std::vector<SonicSample> test_samples {
    SonicSample{
"2023-08-04T12:00:00.0054", 1, 4, R"(b4 dc fe bf 1b 54 25 be 39 22 12 3e 4c 0e 5d 41 00 00 00 00 9e ea 1f 44 25 19 84 40 00 00 00 00 40 e1 4d 41 26 92 a7 42 92 7c 76 3f 19 c5 75 3f 40 4d 21 44 9a f6 dc 00 2d 84 55 aa)",
"2023-08-04T11:59:59.2054", 5, {-1.9911, -0.16145, 0.14271, 13.816, 0, 0.63967, 4.1281, 0, 12.867, 837.85, 0.96284, 0.96004, 645.21, 0, 1.9976, 85.364}
    },
    SonicSample{
"2023-08-04T12:00:00.0214", 1, 4, R"(22 b0 07 c0 74 36 d6 bd 80 6c 15 3e 35 ba 5d 41 00 00 00 00 e4 f5 1f 44 01 47 84 40 00 00 00 00 40 e1 4d 41 26 92 a7 42 4a 7a 76 3f dd c6 75 3f 1e 5a 21 44 9b f6 dc 00 25 5c 55 aa)",
"2023-08-04T11:59:59.2214", 5, {-2.1201, -0.1046, 0.14592, 13.858, 0, 0.63984, 4.1337, 0, 12.867, 837.85, 0.9628, 0.96007, 645.41, 0, 2.1227, 87.176}
    },
    SonicSample{
"2023-08-04T12:00:00.1180", 1, 4, R"(b3 9f 0a c0 18 90 83 3e 8b dd 7b 3e 5a ed 5d 41 00 00 00 00 83 ea 1f 44 37 33 84 40 00 00 00 00 40 e1 4d 41 26 92 a7 42 12 7b 76 3f ed c7 75 3f 1e 4f 21 44 9c f6 dc 00 46 d4 55 aa)",
"2023-08-04T11:59:59.3180", 5, {-2.166, 0.25696, 0.24596, 13.87, 0, 0.63966, 4.1313, 0, 12.867, 837.85, 0.96282, 0.96008, 645.24, 0, 2.1812, 96.766}
    }
};


// Convert the raw data in ascii printable form back to bytes.
static SampleT<char>*
getSampleFromHexStrings(const char* raw_data)
{
    std::vector<uint8_t> data;
    std::istringstream ibuf(raw_data);

    std::string hexstr;
    while (ibuf >> hexstr) {
        uint8_t val = static_cast<uint8_t>(strtol(hexstr.c_str(), nullptr, 16));
        data.push_back(val);
    }
    SampleT<char>* samp = getSample<char>(data.size());
    std::memcpy(samp->getDataPtr(), data.data(), data.size());
    return samp;
}


static void
process_wind3d_samples(CSI_IRGA_Sonic& wind,
    const std::vector<SonicSample>& samples)
{
    for (const auto& sample : samples) {

        SampleT<char>* samp = getSampleFromHexStrings(sample.raw_data);
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

        BOOST_TEST(psamp->getDataLength() == sample.processed_data.size());
        for (size_t i = 0; i < sample.processed_data.size(); ++i) {
            BOOST_CHECK_CLOSE_FRACTION(
                psamp->getDataValue(i), sample.processed_data[i], 0.001);
        }
        psamp->freeReference();
    }
    SamplePools::deleteInstance();
}


BOOST_AUTO_TEST_CASE(test_wind3d_process)
{
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(log_config));

    CSI_IRGA_Sonic wind;

    // first try the standard processing, without despiking and without flags
    setup_wind3d(wind, irga_xml, false);
    process_wind3d_samples(wind, test_samples);
}


BOOST_AUTO_TEST_CASE(test_wind3d_process_with_despike_and_flags)
{
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(log_config));

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


BOOST_AUTO_TEST_CASE(test_wind3d_unpacker)
{
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(log_config));

    CSI_IRGA_Sonic wind;
    setup_wind3d(wind, irga_xml, false);

    for (const auto& sample : test_samples) {
        SampleT<char>* samp = getSampleFromHexStrings(sample.raw_data);

        CSI_IRGA_Fields values;
        const char* buf = samp->getDataPtr();
        const char* eob = samp->getDataPtr() + samp->getDataLength();
        int nvals = wind.unpackBinary(buf, eob, values);
        samp->freeReference();

        // the binary samples have 14 fields, all but Tdetector, even though
        // the test samples only parsed 13, so check that the last is NaN.
        BOOST_TEST(nvals == 14);
        BOOST_CHECK_CLOSE_FRACTION(
            values.u, sample.processed_data[0], 0.001);
        BOOST_CHECK_CLOSE_FRACTION(
            values.v, sample.processed_data[1], 0.001);
        BOOST_CHECK_CLOSE_FRACTION(
            values.w, sample.processed_data[2], 0.001);
        BOOST_CHECK_CLOSE_FRACTION(
            values.tc, sample.processed_data[3], 0.001);
        BOOST_CHECK_CLOSE_FRACTION(values.dPirga,
                                   sample.processed_data[12], 0.001);
        BOOST_TEST(!isnanf(values.Tsource));
        BOOST_TEST(isnanf(values.Tdetector));
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
