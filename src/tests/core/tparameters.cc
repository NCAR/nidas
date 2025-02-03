#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <boost/regex.hpp>

#include <nidas/core/Parameter.h>
#include <nidas/core/XMLParser.h>

using namespace nidas::util;
using namespace nidas::core;
using namespace std;

namespace {
    struct TFixture
    {
        ~TFixture() {
            XMLImplementation::terminate();
        }
    };
}


const char xml_bool[] = R"(
<parameter name='working' type='bool' value='true false 0 1'/>
)";


const char xml_hex[] = R"(
<parameter name='stypes' type='hex' value='0x01 0x80 0xff'/>
)";


const char xml_string[] = R"(
<parameter name='onestring' type='string'
           value='one string to rule them all'/>
)";


const char xml_strings[] = R"(
<parameter name='nstrings' type='strings'
           value='one string to rule them all'/>
)";


BOOST_AUTO_TEST_CASE(test_parameters)
{
    TFixture fix;
    // in the normal case, we can create a parameter using the template
    // subclasses.

    unique_ptr<ParameterT<float> > pfloat{ new ParameterT<float>() };
    pfloat->setValue(99);
    BOOST_TEST(pfloat->getName() == "");
    pfloat->setName("number");
    BOOST_TEST(pfloat->getName() == "number");
    BOOST_TEST(pfloat->getValue(0) == 99);
    BOOST_TEST(pfloat->getFloat(0) == 99);
    BOOST_TEST(pfloat->getInt(0) == 99);
    BOOST_TEST(pfloat->getBool(0) == true);

    BOOST_TEST(pfloat->getStringValue() == "99");

    unique_ptr<xercesc::DOMDocument>
        doc{XMLParser::ParseString(xml_bool)};

    unique_ptr<Parameter> param{
        Parameter::createParameter(doc->getDocumentElement()) };
    BOOST_TEST(param.get());
    BOOST_TEST(param->getName() == "working");
    BOOST_TEST(param->getLength() == 4);
    // false because there is more than one value
    BOOST_TEST(param->getBoolValue() == false);
    BOOST_TEST(param->getNumericValue(0) == 1);
    BOOST_TEST(param->getNumericValue(1) == 0);
    BOOST_TEST(param->getNumericValue(2) == 0);
    BOOST_TEST(param->getNumericValue(3) == 1);

    BOOST_TEST(param->getStringValue() == "1 0 0 1");

    doc.reset(XMLParser::ParseString(xml_string));
    param.reset(Parameter::createParameter(doc->getDocumentElement()));
    BOOST_TEST(param->getName() == "onestring");
    BOOST_TEST(param->getLength() == 1);
    BOOST_TEST(param->getStringValue(0) == "one string to rule them all");

    doc.reset(XMLParser::ParseString(xml_strings));
    param.reset(Parameter::createParameter(doc->getDocumentElement()));
    BOOST_TEST(param->getName() == "nstrings");
    BOOST_TEST(param->getLength() == 6);
    BOOST_TEST(param->getStringValue(0) == "one");
    BOOST_TEST(param->getStringValue(5) == "all");
}


BOOST_AUTO_TEST_CASE(test_value_parameters)
{
    // similar to above, but using a value Parameter.
    TFixture fix;
    // in the normal case, we can create a parameter using the template
    // subclasses.

    Parameter pfloatv("", Parameter::FLOAT_PARAM);
    Parameter* pfloat{&pfloatv};
    pfloat->setValue(99);
    BOOST_TEST(pfloat->getName() == "");
    pfloat->setName("number");
    BOOST_TEST(pfloat->getName() == "number");
    BOOST_TEST(pfloat->getFloat(0) == 99);
    BOOST_TEST(pfloat->getInt(0) == 99);
    BOOST_TEST(pfloat->getBool(0) == true);

    unique_ptr<xercesc::DOMDocument>
        doc{XMLParser::ParseString(xml_bool)};

    Parameter pv;
    Parameter* param{&pv};
    pv.fromDOMElement(doc->getDocumentElement());
    BOOST_TEST(param->getName() == "working");
    BOOST_TEST(param->getLength() == 4);
    // false because there is more than one value
    BOOST_TEST(param->getBoolValue() == false);
    BOOST_TEST(param->getNumericValue(0) == 1);
    BOOST_TEST(param->getNumericValue(1) == 0);
    BOOST_TEST(param->getNumericValue(2) == 0);
    BOOST_TEST(param->getNumericValue(3) == 1);

    doc.reset(XMLParser::ParseString(xml_string));
    param->fromDOMElement(doc->getDocumentElement());
    BOOST_TEST(param->getName() == "onestring");
    BOOST_TEST(param->getLength() == 1);
    BOOST_TEST(param->getStringValue(0) == "one string to rule them all");

    doc.reset(XMLParser::ParseString(xml_strings));
    param->fromDOMElement(doc->getDocumentElement());
    BOOST_TEST(param->getName() == "nstrings");
    BOOST_TEST(param->getLength() == 6);
    BOOST_TEST(param->getStringValue(0) == "one");
    BOOST_TEST(param->getStringValue(5) == "all");
}


BOOST_AUTO_TEST_CASE(test_hex_parameter)
{
    TFixture fix;

    unique_ptr<xercesc::DOMDocument>
        doc{XMLParser::ParseString(xml_hex)};

    Parameter pv;
    pv.fromDOMElement(doc->getDocumentElement());

    BOOST_TEST(pv.getName() == "stypes");
    BOOST_TEST(pv.getType() == Parameter::INT_PARAM);
    BOOST_REQUIRE(pv.getLength() == 3);
    BOOST_TEST(pv.getInt(0) == 1);
    BOOST_TEST(pv.getInt(1) == 128);
    BOOST_TEST(pv.getInt(2) == 255);
}


#ifdef notdef
BOOST_AUTO_TEST_CASE(test_set_value)
{
    // setting a value past the length fills in the rest
    ParameterT<int> ip;
    ip.setValue(3, 5);
    BOOST_TEST(ip.getValues() == vector<int>({0, 0, 0, 5}));
    ip.setValue(5, 5);
    BOOST_TEST(ip.getValues() == vector<int>({0, 0, 0, 5, 0, 5}));
}
#endif


BOOST_AUTO_TEST_CASE(test_virtual_parameters)
{
    unique_ptr<ParameterT<float> > fp{new ParameterT<float>()};

    BOOST_TEST(fp->getLength() == 0);
#ifdef notdef
    fp->setValues(vector<float>({1.5, 2.5, 3.5}));
    BOOST_TEST(fp->getValues() == vector<float>({1.5, 2.5, 3.5}));

    fp->setValue(1, 9.9f);
    BOOST_TEST(fp->getValues() == vector<float>({1.5, 9.9, 3.5}));
#endif

    unique_ptr<ParameterT<int> > ip{new ParameterT<int>()};

    // assignment from different type should silently fail.
    ip->setValue(*fp.get());
    BOOST_TEST(ip->getLength() == 0);
    BOOST_TEST(ip->getType() == Parameter::INT_PARAM);
    BOOST_TEST(fp->getType() == Parameter::FLOAT_PARAM);

    ParameterT<int> i2;
    i2.setValue(10);
    ip->setValue(i2);
    BOOST_TEST(ip->getValue(0) == 10);
    // BOOST_TEST(ip->getValues() == vector<int>({10}));
}
