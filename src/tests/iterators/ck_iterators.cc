/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-12-04 00:15:57 -0700 (Fri, 04 Dec 2009) $

    $LastChangedRevision: 5156 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/apps/ck_xml.cc $
 ********************************************************************
*/

#include <nidas/core/XMLParser.h>
#include <nidas/core/Project.h>
#include <nidas/core/Site.h>

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

int usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " xml_file" << endl;
    return 1;
}

class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};

int main(int argc, char** argv)
{
    if (argc < 2)
      return usage(argv[0]);

    try {

        AutoProject aproject;

	XMLParser* parser = new XMLParser();

	// turn on validation
	parser->setDOMValidation(true);
	parser->setDOMValidateIfSchema(true);
	parser->setDOMNamespaces(true);
	parser->setXercesSchema(true);
	parser->setXercesSchemaFullChecking(true);
        parser->setXercesHandleMultipleImports(true);
	parser->setXercesDoXInclude(true);
	parser->setDOMDatatypeNormalization(false);

	cerr << "parsing: " << argv[1] << endl;
	xercesc::DOMDocument* doc = parser->parse(argv[1]);
	delete parser;

        Project * project = Project::getInstance();
	project->fromDOMElement(doc->getDocumentElement());
        doc->release();

        const int NSITES = 2;
        int nsites = 0;
        for (SiteIterator si = project->getSiteIterator(); si.hasNext(); nsites++)
            si.next();
        if (nsites != NSITES) {
            cerr << "Error: expected " << NSITES << " sites, got " << nsites << endl;
            return 1;
        }

        const int NDSMS = 3;
        int ndsms = 0;
        for (DSMConfigIterator di = project->getDSMConfigIterator(); di.hasNext(); ndsms++)
            di.next();
        if (ndsms != NDSMS) {
            cerr << "Error: expected " << NDSMS << " dsms, got " << ndsms << endl;
            return 1;
        }

        // one <server> at each site, and one global to the <project>
        const int NSERVERS = 3;
        int nservers = 0;
	for (DSMServerIterator si = project->getDSMServerIterator(); si.hasNext(); nservers++)
	    si.next();
        if (nservers != NSERVERS) {
            cerr << "Error: expected " << NSERVERS << " servers, got " << nservers << endl;
            return 1;
        }

        // one <service> for each <server>.
        const int NSERVICES = 3;
        int nservices = 0;
	for (DSMServiceIterator si = project->getDSMServiceIterator(); si.hasNext(); nservices++)
	    si.next();
        if (nservices != NSERVICES) {
            cerr << "Error: expected " << NSERVICES << " services, got " << nservices << endl;
            return 1;
        }

        const int NPROCESSORS = 4;
        // one <processor> at each <site> and two for the <project> <server>.
        int nprocessors = 0;
	for (ProcessorIterator pi = project->getProcessorIterator(); pi.hasNext(); nprocessors++)
	    pi.next();
        if (nprocessors != NPROCESSORS) {
            cerr << "Error: expected " << NPROCESSORS << " processors, got " << nprocessors << endl;
            return 1;
        }

        // 2 sensors on each of the 2 dsms at site "a", 4 sensor on the dsm at site "b".
        const int NSENSORS = 8;
        int nsensors = 0;
        for (SensorIterator si = project->getSensorIterator(); si.hasNext(); nsensors++)
            si.next();
        if (nsensors != NSENSORS) {
            cerr << "Error: expected " << NSENSORS << " sensors, got " << nsensors << endl;
            return 1;
        }

        // 1 samples on each of the 8 sensors, 2 on the <processor> at site "a", 3 on the
        // <processor> at site "b", and 3 on the <processor> at the <project> <server>.
        const int NSAMPLES = 16;
        int nsamples = 0;
        for (SampleTagIterator ti = project->getSampleTagIterator(); ti.hasNext(); nsamples++)
            ti.next();
        if (nsamples != NSAMPLES) {
            cerr << "Error: expected " << NSAMPLES << " samples, got " << nsamples << endl;
            return 1;
        }

        // 2 samples in each sample
        const int NVARIABLES = 32;
        int nvariables = 0;
        for (VariableIterator vi = project->getVariableIterator(); vi.hasNext(); nvariables++)
            vi.next();
        if (nvariables != NVARIABLES) {
            cerr << "Error: expected " << NVARIABLES << " variables, got " << nvariables << endl;
            return 1;
        }

        int isite = 0;
        const int NSERVERS_FOR_SITES[] = {1,1};
        const int NSERVICES_FOR_SITES[] = {1,1};
        const int NPROCESSORS_FOR_SITES[] = {1,1};
        const int NDSMS_FOR_SITES[] = {2,1};
        const int NSENSORS_FOR_SITES[] = {4,4};
        const int NSAMPLES_FOR_SITES[] = {6,7};
        const int NVARIABLES_FOR_SITES[] = {12,14};
        for (SiteIterator si = project->getSiteIterator(); si.hasNext(); isite++) {
            Site* site = si.next();

            nservers = 0;
            for (DSMServerIterator si = site->getDSMServerIterator(); si.hasNext(); nservers++)
                si.next();
            if (nservers != NSERVERS_FOR_SITES[isite]) {
                cerr << "Error: expected " << NSERVERS_FOR_SITES[isite] << " servers for site " <<
                    site->getName() << ", got " << nservers << endl;
                return 1;
            }

            nservices = 0;
            for (DSMServiceIterator si = site->getDSMServiceIterator(); si.hasNext(); nservices++)
                si.next();
            if (nservices != NSERVICES_FOR_SITES[isite]) {
                cerr << "Error: expected " << NSERVICES_FOR_SITES[isite] << " services for site " <<
                    site->getName() << ", got " << nservices << endl;
                return 1;
            }

            nprocessors = 0;
            for (ProcessorIterator pi = site->getProcessorIterator(); pi.hasNext(); nprocessors++)
                pi.next();
            if (nprocessors != NPROCESSORS_FOR_SITES[isite]) {
                cerr << "Error: expected " << NPROCESSORS_FOR_SITES[isite] << " processors for site " <<
                    site->getName() << ", got " << nprocessors << endl;
                return 1;
            }

            ndsms = 0;
            for (DSMConfigIterator di = site->getDSMConfigIterator(); di.hasNext(); ndsms++)
                di.next();
            if (ndsms != NDSMS_FOR_SITES[isite]) {
                cerr << "Error: expected " << NDSMS_FOR_SITES[isite] << " dsms for site " <<
                    site->getName() << ", got " << ndsms << endl;
                return 1;
            }

            nsensors = 0;
            for (SensorIterator si = site->getSensorIterator(); si.hasNext(); nsensors++)
                si.next();
            if (nsensors != NSENSORS_FOR_SITES[isite]) {
                cerr << "Error: expected " << NSENSORS_FOR_SITES[isite] << " sensors for site " <<
                    site->getName() << ", got " << nsensors << endl;
                return 1;
            }

            nsamples = 0;
            for (SampleTagIterator ti = site->getSampleTagIterator(); ti.hasNext(); nsamples++)
                ti.next();
            if (nsamples != NSAMPLES_FOR_SITES[isite]) {
                cerr << "Error: expected " << NSAMPLES_FOR_SITES[isite] << " samples for site " <<
                    site->getName() << ", got " << nsamples << endl;
                return 1;
            }

            nvariables = 0;
            for (VariableIterator vi = site->getVariableIterator(); vi.hasNext(); nvariables++)
                vi.next();
            if (nvariables != NVARIABLES_FOR_SITES[isite]) {
                cerr << "Error: expected " << NVARIABLES_FOR_SITES[isite] << " variables for site " <<
                    site->getName() << ", got " << nvariables << endl;
                return 1;
            }
        }

        const int NSENSORS_FOR_DSMS[] = {2,2,4};
        const int NSAMPLES_FOR_DSMS[] = {2,2,4};
        const int NVARIABLES_FOR_DSMS[] = {4,4,8};
        int idsm = 0;
        for (DSMConfigIterator di = project->getDSMConfigIterator(); di.hasNext(); idsm++) {
            const DSMConfig* dsm = di.next();
            nsensors = 0;
            for (SensorIterator si = dsm->getSensorIterator(); si.hasNext(); nsensors++)
                si.next();
            if (nsensors != NSENSORS_FOR_DSMS[idsm]) {
                cerr << "Error: expected " << NSENSORS_FOR_DSMS[idsm] << " sensors for dsm " <<
                    dsm->getName() << ", got " << nsensors << endl;
                return 1;
            }

            nsamples = 0;
            for (SampleTagIterator ti = dsm->getSampleTagIterator(); ti.hasNext(); nsamples++)
                ti.next();
            if (nsamples != NSAMPLES_FOR_DSMS[idsm]) {
                cerr << "Error: expected " << NSAMPLES_FOR_DSMS[idsm] << " samples for dsm " <<
                    dsm->getName() << ", got " << nsamples << endl;
                return 1;
            }

            nvariables = 0;
            for (VariableIterator vi = dsm->getVariableIterator(); vi.hasNext(); nvariables++)
                vi.next();
            if (nvariables != NVARIABLES_FOR_DSMS[idsm]) {
                cerr << "Error: expected " << NVARIABLES_FOR_DSMS[idsm] << " variables for dsm " <<
                    dsm->getName() << ", got " << nvariables << endl;
                return 1;
            }
        }
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
    cout << "Success: " << argv[0] << endl;
    return 0;
}
