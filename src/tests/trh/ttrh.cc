// -*- c-basic-offset: 4; -*-
#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/core/Project.h>
#include <nidas/util/Process.h>
#include <nidas/dynld/isff/NCAR_TRH.h>
#include <nidas/util/auto_ptr.h>
#include <nidas/util/UTime.h>
#include <cmath> // isnan

using namespace nidas::util;
using namespace nidas::core;
using namespace nidas::dynld::isff;

// 2017 04 20 23:52:06.4888   1.007 34, 103      24          8        nan        nan          5       1391         90 
// 2017 04 20 23:52:06.4888       0 34, 102      34 TRH8 15.65 44.43 5 0 1391 90 16\r\n


BOOST_AUTO_TEST_CASE(test_raw_trh)
{
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(LogConfig()));

    nidas::util::auto_ptr<xercesc::DOMDocument>
	doc(parseXMLConfigFile("test_trh.xml"));

    Project* project = Project::getInstance();
    project->fromDOMElement(doc->getDocumentElement());

    dsm_sample_id_t sid = 0;
    sid = SET_DSM_ID(sid, 34);
    sid = SET_SPS_ID(sid, 102);

    DSMSensor* sensor = project->findSensor(sid);
    BOOST_CHECK(sensor);

    NCAR_TRH* trh = static_cast<NCAR_TRH*>(sensor);
    BOOST_CHECK(trh);
    trh->init();

    // Raw conversions should start out disabled.
    std::vector<float> ta = trh->getRawTempCoefficients();
    BOOST_CHECK_EQUAL(ta.size(), 0);
    std::vector<float> ha = trh->getRawRHCoefficients();
    BOOST_CHECK_EQUAL(ha.size(), 0);

    // Create a raw sample.
    const char* msg = "TRH8 15.65 44.43 5 0 1391 90 16\r\n";
    SampleT<char>* sample = getSample<char>(strlen(msg)+1);
    strcpy(sample->getDataPtr(), msg);
    dsm_time_t tt = UTime(true, 2017, 4, 20, 23, 52, 06.488).toUsecs();
    sample->setTimeTag(tt);

    // Process a sample, which should trigger a read of the cal files
    // and setting up raw conversions.
    std::list<const Sample*> results;
    BOOST_CHECK(trh->process(sample, results));
    BOOST_REQUIRE_EQUAL(results.size(), 1);

    // 2017 apr 20 17:50:33 raw -40.69662       0.04097045      -3.429248e-07
    // Make sure the TRH got the coefficients.
    ta = trh->getRawTempCoefficients();
    BOOST_CHECK_EQUAL(ta.size(), 3);
    BOOST_CHECK_CLOSE(ta[0], -40.69662, 0.0001);
    BOOST_CHECK_CLOSE(ta[1], 0.04097045, 0.0001);
    BOOST_CHECK_CLOSE(ta[2], -3.429248e-07, 0.0001);

    // 2017 apr 20 17:50:33 raw 401.0063 -2.896463 -0.0005389938 -16.3027 0.1413339
    ha = trh->getRawRHCoefficients();
    BOOST_CHECK_EQUAL(ha.size(), 5);
    BOOST_CHECK_CLOSE(ha[0], 401.0063, 0.0001);
    BOOST_CHECK_CLOSE(ha[1], -2.896463, 0.0001);
    BOOST_CHECK_CLOSE(ha[2], -0.0005389938, 0.000001);
    BOOST_CHECK_CLOSE(ha[3], -16.3027, 0.0001);
    BOOST_CHECK_CLOSE(ha[4], 0.1413339, 0.00001);

    // Make sure the values in the sample match the expected conversions.
    const SampleT<float>* outs;
    outs = static_cast<const SampleT<float>*>(results.front());
    BOOST_CHECK(outs);
    const float *fp = outs->getConstDataPtr();
    float Traw = 1391;
    float RHraw = 90;
    float T = trh->tempFromRaw(Traw);
    float RH = trh->rhFromRaw(RHraw, T);
    BOOST_CHECK_CLOSE(fp[1], T, 0.00001);
    BOOST_CHECK_CLOSE(fp[2], RH, 0.00001);
    BOOST_CHECK_EQUAL(fp[4], 1391);
    BOOST_CHECK_EQUAL(fp[5], 90);

    // Test that we can reset raw conversions through the cal file.
    tt = UTime(true, 2017, 4, 21, 8, 15, 0).toUsecs();
    sample->setTimeTag(tt);
    results.front()->freeReference();
    results.clear();
    BOOST_CHECK(trh->process(sample, results));
    BOOST_REQUIRE_EQUAL(results.size(), 1);

    ha = trh->getRawRHCoefficients();
    ta = trh->getRawTempCoefficients();
    BOOST_CHECK_EQUAL(ha.size(), 0);
    BOOST_CHECK_EQUAL(ta.size(), 0);

    outs = static_cast<const SampleT<float>*>(results.front());
    BOOST_CHECK(outs);
    fp = outs->getConstDataPtr();
    BOOST_CHECK(std::isnan(fp[1]));
    BOOST_CHECK(std::isnan(fp[2]));
    BOOST_CHECK_EQUAL(fp[4], Traw);
    BOOST_CHECK_EQUAL(fp[5], RHraw);

    float xta[] = { 1, 2, 3 };
    float xha[] = { 1, 2, 3, 4, 5 };
    trh->setRawTempCoefficients(xta);
    trh->setRawRHCoefficients(xha);
    ha = trh->getRawRHCoefficients();
    ta = trh->getRawTempCoefficients();
    BOOST_CHECK_EQUAL(ta.size(), 3);
    BOOST_CHECK_EQUAL(ha.size(), 5);
    BOOST_CHECK_EQUAL(ta[2], 3);
    BOOST_CHECK_EQUAL(ha[4], 5);

    // Test that we can set and reset raw conversions through the API.
    trh->setRawTempCoefficients();
    trh->setRawRHCoefficients();
    ha = trh->getRawRHCoefficients();
    ta = trh->getRawTempCoefficients();
    BOOST_CHECK_EQUAL(ha.size(), 0);
    BOOST_CHECK_EQUAL(ta.size(), 0);

    results.front()->freeReference();
    results.clear();

    doc.reset();
    XMLImplementation::terminate();
    sample->freeReference();
    Project::destroyInstance();
    SamplePool<float>::deleteInstance();
    SamplePool<char>::deleteInstance();
    nidas::util::Process::clearEnv();
}

