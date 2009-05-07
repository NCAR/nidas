/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-11-28 23:09:46 -0700 (Wed, 28 Nov 2007) $

    $LastChangedRevision: 4060 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/apps/xml_dump.cc $
 ********************************************************************
*/

#include <nidas/core/XMLParser.h>
#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/PortSelectorTest.h>

#include <iostream>
#include <fstream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

void parseAnalog(const DSMConfig * dsm);
void parseOther(const DSMConfig * dsm);

int usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " xml_file" << endl;
    return 1;
}

int main(int argc, char** argv)
{
    if (argc < 2)
      return usage(argv[0]);

    Project * project = 0;
    try {
	cerr << "creating parser" << endl;
	XMLParser * parser = new XMLParser();

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

	for (SiteIterator si = project->getSiteIterator(); si.hasNext(); ) {
	    Site * site = si.next();
            cout << "site:" << site->getName() << endl;

	    for (DSMConfigIterator di = site->getDSMConfigIterator(); di.hasNext(); ) {
		const DSMConfig * dsm = di.next();
                cout << endl << "-----------------------------------------" << endl;
                cout	<< "  dsm: " << dsm->getLocation() << ", ["
			<< dsm->getName() << "]" << endl;

                parseOther(dsm);
                parseAnalog(dsm);
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

void sensorTitle(DSMSensor * sensor)
{
    cout << endl << "    ";
    if (sensor->getCatalogName().length() > 0)
        cout << sensor->getCatalogName();
    else
        cout << sensor->getClassName();

    cout << sensor->getSuffix() << ", " << sensor->getDeviceName();

    const Parameter * parm = sensor->getParameter("SerialNumber");
    if (parm)
        cout << ", SerialNumber " << parm->getStringValue(0);

    CalFile *cf = sensor->getCalFile();
    if (cf) {
        string A2D_SN(cf->getFile());
        A2D_SN = A2D_SN.substr(0,A2D_SN.find(".dat"));
        cout << ", SerialNumber " << A2D_SN;
    }
    cout << ", (" << sensor->getDSMId() << ',' << sensor->getShortId() << ')';

    cout << endl;
}

void parseAnalog(const DSMConfig * dsm)
{
    for (SensorIterator si2 = dsm->getSensorIterator(); si2.hasNext(); ) {
        DSMSensor * sensor = si2.next();

        if (sensor->getClassName().compare("raf.DSMAnalogSensor"))
           continue;

        sensorTitle(sensor);

        cout << "      Samp#  Rate  Variable           gn bi   A/D Cal" << endl;
        const Parameter * parm;
        for (SampleTagIterator ti = sensor->getSampleTagIterator(); ti.hasNext(); ) {
            const SampleTag * tag = ti.next();
            if (!tag->isProcessed()) continue;
            for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); ) {
                const Variable * var = vi.next();
                cout << "        " << tag->getSampleId() << "    ";
		cout.width(4);
		cout << right << tag->getRate() << "  ";
		cout.width(16);
		cout << left << var->getName();

               parm = var->getParameter("gain");
               if (parm) {
                   cout.width(5);
                   cout << right << (int)parm->getNumericValue(0);
               }

               parm = var->getParameter("bipolar");
               if (parm) {
                   cout.width(3);
                   cout << right << (int)parm->getNumericValue(0);
               }

               parm = var->getParameter("corIntercept");
               cout.width(12); cout.precision(6);
               if (parm)
                   cout << right << parm->getNumericValue(0);
               else
                   cout << "";

               parm = var->getParameter("corSlope");
               cout.width(10); cout.precision(6);
               if (parm)
                   cout << right << parm->getNumericValue(0);
               else
                   cout << "";



                cout << endl;
            }
        }
    }
}

void parseOther(const DSMConfig * dsm)
{
    for (SensorIterator si2 = dsm->getSensorIterator(); si2.hasNext(); ) {
        DSMSensor * sensor = si2.next();

        if (sensor->getClassName().compare("raf.DSMAnalogSensor") == 0)
           continue;

        sensorTitle(sensor);

        if (sensor->getCatalogName().compare("IRIG") == 0)
            continue;

        if (sensor->getDeviceName().compare(0, 10, "/dev/arinc") == 0)
            continue;

        cout << "      Samp#  Rate  Variables" << endl;
        for (SampleTagIterator ti = sensor->getSampleTagIterator(); ti.hasNext(); ) {
            const SampleTag* tag = ti.next();
            if (!tag->isProcessed()) continue;

            cout	<< "        " << tag->getSampleId() << "     "
			<< tag->getRate() << "   ";
            int iv = 0;
            for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); iv++) {
                const Variable* var = vi.next();
                if (iv) cout << ',' << var->getName();
                else cout << var->getName();
            }
            cout << endl;
        }
    }
}
