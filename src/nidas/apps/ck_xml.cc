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
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>

#include <iostream>
#include <list>
#include <algorithm>

using namespace nidas::core;
using namespace std;
// using namespace xercesc;

namespace n_u = nidas::util;

class PConfig
{
public:
    PConfig():_xmlFile(),_sensorClasses() {}
    int parseRunstring(int argc, char** argv);
    void usage(const char* argv0);

    int main();

    void showAll(const Project& project);
    void showSensorClasses(const Project& project);

private:
    string _xmlFile;

    list<string> _sensorClasses;
};


int PConfig::parseRunstring(int argc, char** argv)
{
    int opt_char;            /* option character */

    while ((opt_char = getopt(argc, argv, "s:")) != -1) {
	switch (opt_char) {
	case 's':
	    _sensorClasses.push_back(optarg);
	    break;
	case '?':
	    usage(argv[0]);
	    return 1;
	}
    }
    if (optind == argc - 1) _xmlFile = argv[optind++];

    if (optind != argc || _xmlFile.length() == 0) {
	usage(argv[0]);
	return 1;
    }
    return 0;
}

void PConfig::usage(const char* argv0) 
{
    cerr << "Usage: " << argv0 << "[-s sensorClass [-s ...] ] xml_file\n\
    -s sensorClass: display dsm and sensor id for sensors of the given class\n\
" << endl;
}

int PConfig::main()
{

    try {

        Project project;

	XMLParser* parser = new XMLParser();

	// turn on validation
	parser->setDOMValidation(true);
	parser->setDOMValidateIfSchema(true);
	parser->setDOMNamespaces(true);
	parser->setXercesSchema(true);
	parser->setXercesSchemaFullChecking(true);
	parser->setDOMDatatypeNormalization(false);

	cerr << "parsing: " << _xmlFile << endl;
	xercesc::DOMDocument* doc = parser->parse(_xmlFile);
	delete parser;
	project.fromDOMElement(doc->getDocumentElement());
        doc->release();

        if (!_sensorClasses.empty()) showSensorClasses(project);
        else showAll(project);
    }
    catch (const nidas::core::XMLException& e) {
        cerr << e.what() << endl;
        return 1;
    }
    catch (const n_u::InvalidParameterException& e) {
        cerr << e.what() << endl;
        return 1;
    }
    catch (n_u::IOException& e) {
        cerr << e.what() << endl;
        return 1;
    }
    return 0;
}

void PConfig::showAll(const Project& project)
{
    for (SiteIterator si = project.getSiteIterator();
            si.hasNext(); ) {
        Site* site = si.next();
        for (DSMConfigIterator di = site->getDSMConfigIterator();
            di.hasNext(); ) {
            const DSMConfig* dsm = di.next();
            for (SensorIterator si2 = dsm->getSensorIterator(); 
                    si2.hasNext(); ) {
                DSMSensor* sensor = si2.next();
                cout << "site:" << site->getName();
                if (site->getNumber() > 0)
                    cout << ",stn#" << site->getNumber();
                cout << ", sensor: " << sensor->getName() << 
                    '(' << sensor->getDSMId() << ',' <<
                    sensor->getSensorId() << ')' << ',' <<
                    sensor->getClassName() << endl;
                for (SampleTagIterator ti = sensor->getSampleTagIterator();
                    ti.hasNext(); ) {
                    const SampleTag* tag = ti.next();
                    if (!tag->isProcessed()) continue;
                    cout << "  samp#" << tag->getSampleId() << ": ";
                    int iv = 0;
                    for (VariableIterator vi = tag->getVariableIterator();
                        vi.hasNext(); iv++) {
                        const Variable* var = vi.next();
                        if (iv) cout << ',' << var->getName();
                        else cout << var->getName();
                    }
                    cout << endl;
                }
            }
            cout << "-----------------------------------------" << endl;
        }
    }
}

void PConfig::showSensorClasses(const Project& project)
{
    for (SensorIterator si2 = project.getSensorIterator(); si2.hasNext(); ) {
        DSMSensor* sensor = si2.next();
        if (std::find(_sensorClasses.begin(),_sensorClasses.end(),sensor->getClassName()) != _sensorClasses.end())
            cout << sensor->getDSMId() << ',' << sensor->getSensorId() << ' ' <<
                sensor->getClassName() << endl;
    }
}
int main(int argc, char** argv)
{
    PConfig pconfig;

    if (pconfig.parseRunstring(argc,argv)) return 1;

    return pconfig.main();
}
