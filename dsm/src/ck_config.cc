
#include <XMLConfigParser.h>
#include <Project.h>
#include <XMLStringConverter.h>
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

  bool receive(const Sample *s)
  	throw(SampleParseException,atdUtil::IOException)
{
    cerr << dec << "timetag= " << s->getTimeTag() << " id= " << s->getId() <<
    	" len=" << s->getDataLength();
    float* data = (float*) s->getConstVoidDataPtr();

    for (unsigned int i = 0; i < s->getDataLength(); i++)
        std::cerr << ' ' << data[i];
    std::cerr << std::endl;
    return true;
}
};

int main(int argc, char** argv)
{
    TestSampleClient test;

    Project* project = 0;
    PortSelectorTest* handler = 0;
    try {
	cerr << "creating parser" << endl;
	XMLConfigParser* parser = new XMLConfigParser();
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
	    std::cerr << "doing sens->open of" << (*si)->getDeviceName() << std::endl;
	    (*si)->open(O_RDWR);
	    if (si == sensors.begin())
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
