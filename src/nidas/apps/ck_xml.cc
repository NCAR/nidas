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

#include <iostream>

using namespace nidas::core;
using namespace std;
// using namespace xercesc;

namespace n_u = nidas::util;

int usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " xml_file" << endl;
    return 1;
}

int main(int argc, char** argv)
{
    if (argc < 2)
      return usage(argv[0]);

    Project* project = 0;
    try {
	cerr << "creating parser" << endl;
	XMLParser* parser = new XMLParser();

	// turn on validation
	parser->setDOMValidation(true);
	parser->setDOMValidateIfSchema(true);
	parser->setDOMNamespaces(true);
	parser->setXercesSchema(true);
	parser->setXercesSchemaFullChecking(true);
	parser->setDOMDatatypeNormalization(false);
	parser->setXercesUserAdoptsDOMDocument(true);

	cerr << "parsing: " << argv[1] << endl;
	xercesc::DOMDocument* doc = parser->parse(argv[1]);
	cerr << "parsed" << endl;
	cerr << "deleting parser" << endl;
	delete parser;
	project = Project::getInstance();
	cerr << "doing fromDOMElement" << endl;
	project->fromDOMElement(doc->getDocumentElement());
	cerr << "fromDOMElement done" << endl;

	for (SiteIterator si = project->getSiteIterator();
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
                        sensor->getShortId() << ')' << endl;
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

	delete project;
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
