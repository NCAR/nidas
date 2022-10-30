
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/Project.h>
#include <nidas/core/Prompt.h>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <memory>
#include <nidas/util/util.h>

using namespace nidas::util;
using namespace nidas::core;
using nidas::dynld::DSMSerialSensor;

xercesc::DOMDocument*
parseString(const std::string& xml)
{
    XMLParser parser;
    xercesc::MemBufInputSource mbis((const XMLByte *)xml.c_str(),
                    xml.size(), "buffer", false);
    xercesc::DOMDocument* doc = parser.parse(mbis);
    return doc;
}


void
load_prompt_xml(Prompt& prompt, const std::string& pxml)
{
    std::string xml = Project::getInstance()->expandString(pxml);
    std::unique_ptr<xercesc::DOMDocument> doc(parseString(xml));
    prompt.fromDOMElement(doc->getDocumentElement());
    doc.reset();
    XMLImplementation::terminate();
}


void
load_sensor_xml(DSMSerialSensor& dss, const std::string& xml_in)
{
    dss.setDSMId(1);

    std::string xml = Project::getInstance()->expandString(xml_in);
    std::unique_ptr<xercesc::DOMDocument> doc(parseString(xml));
    dss.fromDOMElement(doc->getDocumentElement());
    dss.validate();
    dss.init();

    doc.reset();
    XMLImplementation::terminate();
}



BOOST_AUTO_TEST_CASE(load_prompt_from_xml)
{
    Prompt prompt;
    BOOST_CHECK(!prompt.valid());
    BOOST_TEST(prompt.toXML() == "<prompt/>");
    std::string xml = "<prompt string='4D0!' rate='0.002' offset='55'/>";
    load_prompt_xml(prompt, xml);
    BOOST_CHECK_EQUAL(prompt.getString(), "4D0!");
    BOOST_CHECK_EQUAL(prompt.getRate(), 0.002);
    BOOST_CHECK_EQUAL(prompt.getOffset(), 55);
    BOOST_TEST(prompt.toXML() == xml);
    BOOST_CHECK(prompt.valid());

    prompt = Prompt();
    BOOST_CHECK(!prompt.valid());
    load_prompt_xml(prompt,
                    "<prompt string='HERE:'/>");
    BOOST_CHECK_EQUAL(prompt.getString(), "HERE:");
    BOOST_CHECK_EQUAL(prompt.getRate(), 0.0);
    BOOST_CHECK_EQUAL(prompt.getOffset(), 0);
    BOOST_CHECK(prompt.hasPrompt());
    // prompt without a rate is invalid, even with a prompt string.
    BOOST_CHECK(!prompt.valid());
}


BOOST_AUTO_TEST_CASE(invalid_prompt_xml_throws)
{
    Prompt prompt;

    std::vector<std::string> xmls { 
        "<prompt string='HERE:' offset=''/>",
        "<prompt string='HERE:' rate=''/>",
        "<prompt string='HERE:' rate='-1'/>",
        "<prompt string='HERE:' rate='x'/>"
    };

    for (auto xml: xmls)
    {
        try {
            load_prompt_xml(prompt, xml);
            BOOST_FAIL(std::string("exception not thrown: ") + xml);
        }
        catch (const InvalidParameterException& ipe)
        {
            BOOST_CHECK(true);
        }
    }
}


BOOST_AUTO_TEST_CASE(prompt_xml_defaults)
{
    // check that unspecified attributes are reset to defaults.
    Prompt prompt;
    load_prompt_xml(prompt, "<prompt string='4D0!'/>");
    BOOST_TEST(prompt.getString() == "4D0!");
    BOOST_TEST(prompt.getRate() == 0);
    BOOST_TEST(prompt.getOffset() == 0);

    load_prompt_xml(prompt, "<prompt rate='2'/>");
    BOOST_TEST(prompt.getString() == "");
    BOOST_TEST(prompt.getRate() == 2);
    BOOST_TEST(prompt.getOffset() == 0);

    load_prompt_xml(prompt, "<prompt offset='50'/>");
    BOOST_TEST(prompt.getString() == "");
    BOOST_TEST(prompt.getRate() == 0);
    BOOST_TEST(prompt.getOffset() == 50);
}


// XML for a snow pillow sensor with complicated prompting.
const char* pillow_xml = R"XML(
<serialSensor ID="PILLOWS" class="DSMSerialSensor"
              baud="9600" parity="none" databits="8" stopbits="1"
              devicename="/dev/ttySPIUSB" id="1050" suffix="">

    <prompt rate="0.002"/>
    <sample id="18" scanfFormat="4%f%f%f">
        <variable name="SWE.p4" units="cm" longname="Snow Water Equivalent" plotrange="0 50"></variable>
        <variable name="Text.p4" units="degC" longname="External temperature" plotrange="$T_RANGE"></variable>
        <variable name="Tint.p4" units="degC" longname="Internal temperature" plotrange="$T_RANGE"></variable>
        <prompt string="4D0!" offset="55"/>
    </sample>

</serialSensor>
)XML";


BOOST_AUTO_TEST_CASE(prompt_inherits_rates)
{
    // make sure a sample prompt can inherit the rate of the sensor prompt.
    DSMSerialSensor dss;

    load_sensor_xml(dss, pillow_xml);
    BOOST_CHECK(dss.getPrompt() == Prompt("", 0.002));
    BOOST_TEST(! dss.getPrompt().valid());

    std::list<SampleTag*>& stags = dss.getSampleTags();
    BOOST_REQUIRE(stags.size() == 1);

    SampleTag& tag = *(stags.front());
    // the sample should have a rate which matches the sensor prompt, since it
    // wasn't set otherwise, and the sample prompt also inherits the rate.
    BOOST_TEST(tag.getPrompt() == Prompt("4D0!", 0.002, 55));
    BOOST_TEST(tag.getRate() == 0.002);

    // there should be one prompt associated with this sensor
    const std::list<Prompt>& prompts = dss.getPrompts();
    BOOST_TEST(prompts.size() == 1);
}


const char* mismatch_rate_xml = R"XML(
<serialSensor class="DSMSerialSensor"
              baud="9600" parity="none" databits="8" stopbits="1"
              devicename="/dev/ttySPIUSB" id="1050" suffix="">

    <sample id="18" scanfFormat="4%f%f%f" rate='20'>
        <variable name="SWE.p4" units="cm" longname="Snow Water Equivalent" plotrange="0 50"></variable>
        <variable name="Text.p4" units="degC" longname="External temperature" plotrange="$T_RANGE"></variable>
        <variable name="Tint.p4" units="degC" longname="Internal temperature" plotrange="$T_RANGE"></variable>
        <prompt rate='10' string="4D0!" offset="55"/>
    </sample>

</serialSensor>
)XML";


BOOST_AUTO_TEST_CASE(prompt_rate_mismatch)
{
    DSMSerialSensor dss;

    try {
        load_sensor_xml(dss, mismatch_rate_xml);
        BOOST_FAIL("exception not thrown for rate mismatch");
    }
    catch (InvalidParameterException& ipe)
    {
        BOOST_CHECK(true);
    }
}
