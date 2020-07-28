// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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
/*
 * Read an XML configuration, find an instance of a DSC_A2DSensor,
 * which interfaces to a Diamond Systems Corporation Analog I/O module.
 * Open the sensor instance using the config in the XML, and read
 * samples back.  This is the start of a program for doing
 * calibrations on the A2D.
*/

#include <fstream>
#include <sys/stat.h>

#include <unistd.h>
#include <getopt.h>
#include <iomanip>

#include <nidas/util/Process.h>
#include <nidas/core/NidasApp.h>
#include <nidas/core/Project.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/dynld/DSC_A2DSensor.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

// const n_u::EndianConverter * littleEndian = n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_LITTLE_ENDIAN);

class DmdA2dCk
{
public:

    DmdA2dCk();

    static int main(int argc, char** argv) throw();

    int parseRunstring(int argc, char** argv);

    int usage(const char* argv0);

    int run() throw();

private:
    NidasApp _app;

};

DmdA2dCk::DmdA2dCk():
    _app("data_stats")
{
    _app.setApplicationInstance();
    _app.setupSignals();
    _app.enableArguments(_app.XmlHeaderFile | _app.loggingArgs() |
                        _app.Version | _app.Help);
}

int main(int argc, char** argv)
{
    return DmdA2dCk::main(argc,argv);
}


int DmdA2dCk::parseRunstring(int argc, char** argv)
{
 try {
        ArgVector args = _app.parseArgs(argc, argv);
        if (_app.helpRequested())
        {
            return usage(argv[0]);
        }
        _app.parseInputs(args);
    }
    catch (NidasAppException& ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    return 0;
}

int DmdA2dCk::usage(const char* argv0)
{
    cerr <<
        "Usage: " << argv0 << " [options] [inputURL] ...\n";
    cerr <<
        "Standard options:\n"
         << _app.usage() <<
        "Example:\n" <<
        argv0 << " -x dmd.xml\n" << endl;
    return 1;
}

/* static */
int DmdA2dCk::main(int argc, char** argv) throw()
{
    DmdA2dCk a2dck;
    int result;
    if (! (result = a2dck.parseRunstring(argc, argv)))
    {
        try {
            result = a2dck.run();
        }
        catch (n_u::Exception& e)
        {
            cerr << e.what() << endl;
            XMLImplementation::terminate(); // ok to terminate() twice
            result = 1;
        }
    }
    return result;
}

class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};

int DmdA2dCk::run() throw()
{
    AutoProject aproject;
    try
    {
	string xmlFileName = _app.xmlHeaderFile();

        xmlFileName = n_u::Process::expandEnvVars(xmlFileName);

        n_u::auto_ptr<xercesc::DOMDocument> doc(parseXMLConfigFile(xmlFileName));

        Project::getInstance()->fromDOMElement(doc->getDocumentElement());

        DSMConfigIterator di = Project::getInstance()->getDSMConfigIterator();
        DSC_A2DSensor *a2d = 0;

        for ( ; di.hasNext(); )
        {
            const DSMConfig * dsm = di.next();
            const list<DSMSensor *>& sensors = dsm->getSensors();

            list<DSMSensor *>::const_iterator dsm_it;
            for (dsm_it = sensors.begin(); dsm_it != sensors.end(); ++dsm_it)
            {
                a2d = dynamic_cast<DSC_A2DSensor *>((*dsm_it));
                if ( ! a2d ) continue;

                cerr << "Found DSC_A2DSensor = " << (*dsm_it)->getDeviceName()
                     << endl;

                a2d->validate();

                // const Parameter * parm = p->sensor->getParameter("RESOLUTION");
                a2d->open(O_RDONLY);
                a2d->init();
            }
        }
        if (!a2d) {
            cerr << "DSC_A2DSensor not found in XML" << endl;
            return 1;
        }

        try {
            for (;;) {
                if (_app.interrupted()) break;
                a2d->readBuffer();
                const Sample *samp;
                while ((samp = a2d->nextSample())) {
                    // dsm_sample_id_t id = samp->getId();
                    list<const Sample*> results;

                    // process sample
                    a2d->process(samp,results);

                    // iterate over processed samples
                    list<const Sample*>::const_iterator psi = results.begin();
                    for ( ; psi != results.end(); ++psi) {
                        const Sample *psamp = *psi;
                        int nval = psamp->getDataLength();
                        for (int i = 0; i < nval; i++) {
                            double val = psamp->getDataValue(i);
                            cerr << "val=" << val << endl;
                        }
                        // free processed sample
                        psamp->freeReference();
                    }

                    // free raw sample
                    samp->freeReference();
                }
            }
        }
        catch (n_u::IOException& ioe) {
            cerr << ioe.what() << endl;
        }

        a2d->close();
    }
    catch (n_u::IOException& ioe)
    {
        cerr << ioe.what() << endl;
	return 1;
    }
    return 0;
}

