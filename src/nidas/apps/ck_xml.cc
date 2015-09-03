/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include <nidas/core/XMLParser.h>
#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/core/VariableConverter.h>
#include <nidas/core/CalFile.h>

#include <iostream>
#include <list>
#include <algorithm>

#include <unistd.h>
#include <getopt.h>

using namespace nidas::core;
using namespace std;
// using namespace xercesc;

namespace n_u = nidas::util;

class PConfig
{
public:
    PConfig():_xmlFile(),_sensorClasses(),_showCalFiles(false) {}
    int parseRunstring(int argc, char** argv);
    void usage(const char* argv0);

    int main();

    void showAll(const Project& project);
    void showSensorClasses(const Project& project);
    void showCalFiles(const Project& project);

private:
    string _xmlFile;

    list<string> _sensorClasses;

    bool _showCalFiles;
};


int PConfig::parseRunstring(int argc, char** argv)
{
    int opt_char;            /* option character */

    while ((opt_char = getopt(argc, argv, "cs:")) != -1) {
	switch (opt_char) {
	case 'c':
	    _showCalFiles = true;
	    break;
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
    -c: display a listing of all cal files referenced in the xml\n\
    -s sensorClass: display dsm and sensor id for sensors of the given class\n\
" << endl;
}

int PConfig::main()
{

    int res = 0;
    try {

        Project project;

	XMLParser parser;

	// turn on validation
	parser.setDOMValidation(true);
	parser.setDOMValidateIfSchema(true);
	parser.setDOMNamespaces(true);
	parser.setXercesSchema(true);
	parser.setXercesSchemaFullChecking(true);
        parser.setXercesHandleMultipleImports(true);
	parser.setXercesDoXInclude(true);
	parser.setDOMDatatypeNormalization(false);

	cerr << "parsing: " << _xmlFile << endl;
	xercesc::DOMDocument* doc = parser.parse(_xmlFile);
	project.fromDOMElement(doc->getDocumentElement());
        doc->release();

        if (!_sensorClasses.empty()) showSensorClasses(project);
        else if (_showCalFiles) showCalFiles(project);
        else showAll(project);
    }
    catch (const nidas::core::XMLException& e) {
        cerr << e.what() << endl;
        res = 1;
    }
    catch (const n_u::InvalidParameterException& e) {
        cerr << e.what() << endl;
        res = 1;
    }
    catch (n_u::IOException& e) {
        cerr << e.what() << endl;
        res = 1;
    }
    XMLImplementation::terminate();
    return res;
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

void PConfig::showCalFiles(const Project& project)
{
    for (SiteIterator si = project.getSiteIterator();
            si.hasNext(); ) {
        Site* site = si.next();
        const list<DSMConfig*>& dsms = site->getDSMConfigs();
        list<DSMConfig*>::const_iterator di = dsms.begin();

        for (di = dsms.begin(); di != dsms.end(); ++di) {
            DSMConfig* dsm = *di;
            const list<DSMSensor*>& sensors = dsm->getSensors();
            list<DSMSensor*>::const_iterator si2;
            for (si2 = sensors.begin(); si2 != sensors.end(); ++si2) {
                DSMSensor* sensor = *si2;

                const map<string,CalFile*>& cfs = sensor->getCalFiles();

                if (!cfs.empty()) {
                    map<string,CalFile*>::const_iterator ci = cfs.begin();
                    cout << "site: " << site->getName() << ", dsm: " << dsm->getName() <<
                        ", sensor: " << sensor->getCatalogName() <<
                        ' ' << sensor->getClassName() <<  ' ' <<
                        sensor->getDeviceName() << ' ' << sensor->getHeightString();
                    for ( ; ci != cfs.end(); ++ci) {
                        CalFile* cf = ci->second;
                        try {
                            cf->open();
                            cout << ", calfile: " << cf->getCurrentFileName();
                        }
                        catch(const n_u::IOException&e) {
                            cout << ", calfile: " << e.what();
                        }
                    }
                    cout << endl;
                }
                const list<SampleTag*>& tags = sensor->getSampleTags();
                list<SampleTag*>::const_iterator ti;
                for (ti = tags.begin(); ti != tags.end(); ++ti) {
                    SampleTag* tag = *ti;
                    const vector<Variable*> vars = tag->getVariables();
                    vector<Variable*>::const_iterator vi;
                    for (vi = vars.begin(); vi != vars.end(); ++vi) {
                        Variable* var = *vi;
                        VariableConverter* vc = var->getConverter();
                        if (vc) {
                            CalFile* cf = vc->getCalFile();
                            if (cf) {
                                cout << "site: " << site->getName() << ", dsm: " << dsm->getName() <<
                                    ", sensor: " << sensor->getCatalogName() << ' ' << sensor->getClassName() <<
                                    ' ' << sensor->getDeviceName() << ", variable: " << var->getName();
                                try {
                                    cf->open();
                                    cout << ", calfile: " << cf->getCurrentFileName();
                                }
                                catch(const n_u::IOException&e) {
                                    cout << ", calfile: " << e.what();
                                }
                                cout << endl;
                            }
                        }
                    }
                }
            }
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
