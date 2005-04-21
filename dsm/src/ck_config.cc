/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <XMLParser.h>
#include <Project.h>
#include <Aircraft.h>
// #include <DSMConfig.h>
#include <PortSelectorTest.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

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
	DOMDocument* doc = parser->parse(argv[1]);
	cerr << "parsed" << endl;
	project = Project::getInstance();
	project->fromDOMElement(doc->getDocumentElement());
	cerr << "deleting parser" << endl;
	delete parser;

	handler = PortSelectorTest::createInstance();
	handler->start();

	const list<Aircraft*>& aclist = project->getAircraft();
	if (!aclist.size()) goto done;

	Aircraft* aircraft = aclist.front();

	const list<DSMConfig*>& dsms = aircraft->getDSMConfigs();

	if (!dsms.size()) goto done;

	DSMConfig* dsm = dsms.front();

	const list<DSMSensor*>& sensors = dsm->getSensors();

	list<DSMSensor*>::const_iterator si;

	for (si = sensors.begin(); si != sensors.end(); ++si) {
	    std::cerr << "doing sens->open of" <<
	    	(*si)->getDeviceName() << endl;
	    (*si)->open((*si)->getDefaultMode());
	    (*si)->addSampleClient(&test);
	    handler->addSensorPort(*si);
	}
    }
    catch (const DOMException& e) {
        cerr << XMLStringConverter(e.getMessage()) << endl;
    }
    catch (const XMLException& e) {
        cerr << XMLStringConverter(e.getMessage()) << endl;
    }
    catch (const atdUtil::InvalidParameterException& e) {
        cerr << e.what() << endl;
    }
    catch (atdUtil::IOException& ioe) {
      std::cerr << ioe.what() << std::endl;
      throw atdUtil::Exception(ioe.what());
    }

done:
    try {
	handler->join();
    }
    catch(atdUtil::Exception& e) {
        cerr << "join exception: " << e.what() << endl;
	return 1;
    }
    cerr << "deleting project" << endl;
    delete project;
    delete handler;
    return 0;
}
