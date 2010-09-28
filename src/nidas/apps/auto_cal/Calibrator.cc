#include "Calibrator.h"
#include "AutoCalClient.h"

#include <sys/stat.h>

#include <nidas/core/Socket.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/IOChannel.h>
#include <nidas/core/Project.h>

#include <nidas/util/EOFException.h>
#include <nidas/util/Process.h>

#include <nidas/dynld/raf/DSMAnalogSensor.h>

#ifdef SIMULATE
#include <nidas/core/FileSet.h>
#endif

#include <QMessageBox>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;
using namespace XmlRpc;

namespace n_u = nidas::util;

string stateEnumDesc[] = {"GATHER", "DONE", "TEST", "DEAD" };

class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};


Calibrator::Calibrator( AutoCalClient *acc ):
   testVoltage(false),
   cancel(false),
   _acc(acc),
   _sis(0),
   _pipeline(0)
{
    AutoProject project;
}


Calibrator::~Calibrator()
{
    cout << "Calibrator::~Calibrator" << endl;

    if (_pipeline)
        _pipeline->getProcessedSampleSource()->removeSampleClient(_acc);

    if (isRunning()) {
        canceled();
        wait();
    }
    delete _sis;
    delete _pipeline;
};


bool Calibrator::setup() throw()
{
    cout << "Calibrator::setup()" << endl;

    try {
        IOChannel* iochan = 0;

#ifdef SIMULATE
        // local data file is a copy of:
        // /scr/raf/Raw_Data/PLOWS/20091106_161332_ff04.ads
        list<string> dataFileNames;
        dataFileNames.push_back("/home/data/20091106_161332_ff04.ads");
        nidas::core::FileSet* fset =
            nidas::core::FileSet::getFileSet(dataFileNames);
        iochan = fset->connect();
        cout << "SIMULATE!  using " << dataFileNames.front() << endl;
#else
        // real time operation
        auto_ptr<n_u::SocketAddress> sockAddr;
        n_u::Inet4Address addr = n_u::Inet4Address::getByName("localhost");
        sockAddr.reset(new n_u::Inet4SocketAddress(addr,30000));
        n_u::Socket* sock = new n_u::Socket(*sockAddr.get());
        iochan = new nidas::core::Socket(sock);
        cout << "Calibrator::setup() connected to dsm_server" << endl;
#endif

        _sis = new RawSampleInputStream(iochan);
        cout << "_sis:      " << _sis << endl;
        _sis->setMaxSampleLength(32768);
        _sis->readInputHeader();

        cout << "Calibrator::setup() RawSampleStream now owns the iochan ptr." << endl;

        const SampleInputHeader& header = _sis->getInputHeader();

        string xmlFileName = header.getConfigName();
        xmlFileName = n_u::Process::expandEnvVars(xmlFileName);

        struct stat statbuf;
        if ( ::stat(xmlFileName.c_str(),&statbuf) ) {
            ostringstream ostr;
            ostr << "Configuration file: '" << xmlFileName;
            ostr << "' not found!" << endl;
            QMessageBox::critical(0, "CANNOT start", ostr.str().c_str());
            return true;
        }
        cout << "Calibrator::setup() found xml config file" << endl;
        auto_ptr<xercesc::DOMDocument> doc(
          DSMEngine::parseXMLConfigFile(xmlFileName));

        Project::getInstance()->fromDOMElement(doc->getDocumentElement());

        bool noneFound = true;

        _pipeline = new SamplePipeline();
        cout << "_pipeline: " << _pipeline << endl;

        DSMConfigIterator di = Project::getInstance()->getDSMConfigIterator();
        for ( ; di.hasNext(); ) {
            const DSMConfig* dsm = di.next();
            const list<DSMSensor*>& allSensors = dsm->getSensors();

            list<DSMSensor*>::const_iterator si;
            for (si = allSensors.begin(); si != allSensors.end(); ++si) {
                DSMSensor* sensor = *si;

                // skip non-Analog type sensors
                if (sensor->getClassName().compare("raf.DSMAnalogSensor"))
                    continue;

                // skip non-responsive of miss-configured sensors
                if ( _acc->Setup(sensor) )
                    continue;

                dsmLocations[dsm->getId()] = dsm->getLocation();

                // initialize the sensor
                if (!testVoltage)
                    sensor->setCalFile(0);
                sensor->init();

                //  inform the SampleInputStream of what SampleTags to expect
                cout << "_sis->addSampleTag(sensor->getRawSampleTag());" << endl;
                _sis->addSampleTag(sensor->getRawSampleTag());

                // connect to the _pipeline member
                _pipeline->connect(sensor);

                noneFound = false;
            }
        }
        if ( noneFound ) {
            ostringstream ostr;
            ostr << "No analog cards available to calibrate!";
            QMessageBox::critical(0, "no cards", ostr.str().c_str());
            return true;
        }
        cout << "Calibrator::setup() extracted analog sensors" << endl;
        _acc->createQtTreeModel(dsmLocations);
    }
    catch (n_u::IOException& e) {
        ostringstream ostr;
        ostr << "DSM server is not running!" << endl;
        ostr << "You need to start NIDAS" << endl;
        QMessageBox::critical(0, "CANNOT start", ostr.str().c_str());
        return true;
    }
    cout << "Calibrator::setup() FINISHED" << endl;
    return false;
}


void Calibrator::run()
{
    cout << "Calibrator::run()" << endl;

    try {
        _pipeline->setRealTime(true);
        _pipeline->setProcSorterLength(0);

        // 2. connect the pipeline to the SampleInputStream.
        _pipeline->connect(_sis);

        // 3. connect the client to the pipeline
        _pipeline->getProcessedSampleSource()->addSampleClient(_acc);

        try {
            enum stateEnum state = GATHER;
            if (testVoltage) state = TEST;

            while ( (state = _acc->SetNextCalVoltage(state)) != DONE ) {

                cout << "state: " << stateEnumDesc[state] << endl;

                if (state == DONE)
                    break;

                if (state == DEAD)
                    break;

                cout << "gathering..." << endl;
                while ( testVoltage || !_acc->Gathered() ) {

                    if (cancel) {
                        cout << "canceling..." << endl;
                        state = DONE;
                        break;
                    }
                    _sis->readSamples();

                    // update progress bar
                    if (!testVoltage)
                        emit setValue(_acc->progress);
                }
            }
            if (testVoltage) state = TEST;
            if (state == DONE) {
                _acc->DisplayResults();

                // update progress bar
                emit setValue(_acc->progress);
            }
        }
        catch (n_u::EOFException& e) {
            cerr << e.what() << endl;
            _sis->flush();
            _sis->close();
        }
        catch (n_u::IOException& e) {
            _pipeline->getProcessedSampleSource()->removeSampleClient(_acc);
            _pipeline->disconnect(_sis);
            _sis->close();
            throw(e);
        }
        _pipeline->getProcessedSampleSource()->removeSampleClient(_acc);
        _pipeline->disconnect(_sis);
        _sis->close();
    }
    catch (n_u::IOException& e) {
        cerr << e.what() << endl;
    }
    catch (n_u::Exception& e) {
        cerr << e.what() << endl;
    }
    cout << "Calibrator::run() FINISHED" << endl;
}


void Calibrator::canceled()
{
    cancel = true;
}
