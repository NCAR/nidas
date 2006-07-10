/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/XMLParser.h>
#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
// #include <DSMConfig.h>
#include <nidas/core/PortSelectorTest.h>

#include <iostream>

using namespace nidas::core;
using namespace std;
// using namespace xercesc;

namespace n_u = nidas::util;


/**
 * A little SampleClient for testing purposes.  Currently
 * just prints out some fields from the Samples it receives.
 */
class TestSampleClient : public SampleClient {
public:

    TestSampleClient() : tstat(0),tlast(0),cnt(0),period(60000) {}

    bool receive(const Sample *s) throw()
    {
	dsm_sample_time_t tt = s->getTimeTag();
#ifdef DEBUG
	cerr << dec << "timetag= " << tt <<
		" id= " << s->getId() << " len=" << s->getDataLength();
	float* data = ((FloatSample*)s)->getDataPtr();

	for (unsigned int i = 0; i < s->getDataLength(); i++)
	    std::cerr << ' ' << data[i];
	std::cerr << std::endl;
#endif
	if (tt < tlast && tlast > 86390000 && 
		tt < 10000 && tstat == 86400000)
	    tstat = 0;
	if (tt > tstat) {
	    std::cerr << "tstat=" << tstat << " cnt=" << cnt <<
	        " cnt/sec=" << (cnt * 1000.) / period << std::endl;
	    cnt = 0;
	    tstat = ((tt / period) + 1) * period;
	}
	tlast = tt;
	cnt++;
	return true;
    }
private:
    dsm_sample_time_t tstat;
    dsm_sample_time_t tlast;
    int cnt;
    int period;
};

int main(int argc, char** argv)
{
    TestSampleClient test;

    Project* project = 0;
    PortSelectorTest* handler = 0;
    try {
	cerr << "creating parser" << endl;
	XMLParser* parser = new XMLParser();
	xercesc::DOMDocument* doc = parser->parse(argv[1]);
	cerr << "parsed" << endl;
	project = Project::getInstance();
	project->fromDOMElement(doc->getDocumentElement());
	cerr << "deleting parser" << endl;
	delete parser;

	handler = PortSelectorTest::createInstance();
	handler->start();

	const list<Site*>& sitelist = project->getSites();
	if (!sitelist.size()) goto done;

	Site* site = sitelist.front();

	const list<const DSMConfig*>& dsms = site->getDSMConfigs();

	if (!dsms.size()) goto done;

	const DSMConfig* dsm = dsms.front();

	const list<DSMSensor*>& sensors = dsm->getSensors();

	list<DSMSensor*>::const_iterator si;

	for (si = sensors.begin(); si != sensors.end(); ++si) {
	    DSMSensor* sensor = *si;
	    std::cerr << "doing sens->open of" <<
	    	sensor->getDeviceName() << endl;
	    sensor->open(sensor->getDefaultMode());
	    sensor->addSampleClient(&test);
	    handler->addSensor(sensor);
	}
    }
    catch (const nidas::core::XMLException& e) {
        cerr << e.what() << endl;
    }
    catch (const n_u::InvalidParameterException& e) {
        cerr << e.what() << endl;
    }
    catch (n_u::IOException& ioe) {
      std::cerr << ioe.what() << std::endl;
      throw n_u::Exception(ioe.what());
    }

done:
    try {
	handler->join();
    }
    catch(n_u::Exception& e) {
        cerr << "join exception: " << e.what() << endl;
	return 1;
    }
    cerr << "deleting project" << endl;
    delete project;
    delete handler;
    return 0;
}
