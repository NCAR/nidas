// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2014-08-11 14:43:37 -0600 (Mon, 11 Aug 2014) $

    $LastChangedRevision: 7093 $

    $LastChangedBy: granger $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/apps/sync_server.cc $
 ********************************************************************

*/

#include "SyncServer.h"

#include <ctime>

#include <nidas/core/FileSet.h>
#include <nidas/dynld/SampleOutputStream.h>
#include <nidas/core/SampleOutputRequestThread.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/util/Process.h>
#include <nidas/util/Logger.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

using nidas::dynld::raf::SyncServer;

SyncServer::SyncServer():
    Thread("SyncServer"),
    project(), pipeline(), syncGen(), 
    _inputStream(0), _outputStream(0),
    _xmlFileName(), _dataFileNames(),
    _address(new n_u::Inet4SocketAddress(DEFAULT_PORT)),
    _sorterLengthSecs(SORTER_LENGTH_SECS),
    _sampleClient(0),
    _stop_signal(0)
{
}

#ifdef PROJECT_IS_SINGLETON
class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};
#endif

void
SyncServer::
initProject()
{
    auto_ptr<xercesc::DOMDocument> doc(parseXMLConfigFile(_xmlFileName));
    project.fromDOMElement(doc->getDocumentElement());
    // XMLImplementation::terminate();
}


void
SyncServer::
initSensors(SampleInputStream& sis)
{
    set<DSMSensor*> sensors;
    SensorIterator ti = project.getSensorIterator();
    for ( ; ti.hasNext(); ) {
        DSMSensor* sensor = ti.next();
        if (sensors.insert(sensor).second) {
            sis.addSampleTag(sensor->getRawSampleTag());
            sensors.insert(sensor);
            sensor->init();
        }
    }
}


int
SyncServer::
run() throw(n_u::Exception)
{
    try {
        read();
    }
    catch (n_u::Exception& e) {
        stop();
        cerr << e.what() << endl;
        XMLImplementation::terminate(); // ok to terminate() twice
	return 1;
    }
    return 0;
}


void
SyncServer::
interrupt()
{
    Thread::interrupt();
    pipeline.interrupt();
}


void
SyncServer::
stop()
{
    // This should not throw any exceptions since it gets called from
    // within exception handlers.
    _inputStream->close();
    syncGen.disconnect(pipeline.getProcessedSampleSource());
    if (_outputStream)
        syncGen.disconnect(_outputStream);
    pipeline.interrupt();
    pipeline.join();
    SampleOutputRequestThread::destroyInstance();
    // Can't do this here since there may be other objects (eg
    // SyncRecordReader) still holding onto samples.  Programs which want
    // to do this cleanup (like sync_server) need to do it on their own
    // once all the Sample users are done.
    //
    //    SamplePools::deleteInstance();
    delete _inputStream;
    delete _outputStream;
    _inputStream = 0;
    _outputStream = 0;
    if (_stop_signal)
    {
        _stop_signal->stop();
        delete _stop_signal;
        _stop_signal = 0;
    }
}


void
SyncServer::
init() throw(n_u::Exception)
{
    IOChannel* iochan = 0;

    nidas::core::FileSet* fset =
        nidas::core::FileSet::getFileSet(_dataFileNames);

    iochan = fset->connect();

    // RawSampleStream owns the iochan ptr.
    _inputStream = new RawSampleInputStream(iochan);
    RawSampleInputStream& sis = *_inputStream;

    // Apply some sample filters in case the file is corrupted.
    sis.setMaxDsmId(2000);
    sis.setMaxSampleLength(64000);
    sis.setMinSampleTime(n_u::UTime::parse(true,"2006 jan 1 00:00"));
    sis.setMaxSampleTime(n_u::UTime::parse(true,"2020 jan 1 00:00"));

    sis.readInputHeader();
    SampleInputHeader header = sis.getInputHeader();
    if (_xmlFileName.length() == 0)
        _xmlFileName = header.getConfigName();
    _xmlFileName = n_u::Process::expandEnvVars(_xmlFileName);

    initProject();
    initSensors(sis);

    pipeline.setRealTime(false);
    pipeline.setRawSorterLength(1.0);
    pipeline.setProcSorterLength(_sorterLengthSecs);
	
    // Even though the time length of the raw sorter is typically
    // much smaller than the length of the processed sample sorter,
    // (currently 1 second vs 900 seconds) its heap size needs to be
    // proportionally larger since the raw samples include the fast
    // 2DC data, and the processed 2DC samples are much smaller.
    // Note that if more memory than this is needed to sort samples
    // over the length of the sorter, then the heap is dynamically
    // increased. There isn't much penalty in choosing too small of
    // a value.
    pipeline.setRawHeapMax(50 * 1000 * 1000);
    pipeline.setProcHeapMax(100 * 1000 * 1000);
    pipeline.connect(&sis);

    syncGen.connect(pipeline.getProcessedSampleSource());

    // SyncRecordGenerator is now connected to the pipeline output, all
    // that remains is connecting the output of the generator.  By default
    // the output goes to a socket, but that will be disabled if another
    // SampleClient instance (ie SyncRecordReader) has been specified
    // instead.
    if (! _sampleClient)
    {
        nidas::core::ServerSocket* servSock = 
            new nidas::core::ServerSocket(*_address.get());
        IOChannel* ioc = servSock->connect();
        if (ioc != servSock) {
            servSock->close();
            delete servSock;
        }

        _outputStream = new SampleOutputStream(ioc,&syncGen);
        SampleOutputStream& output = *_outputStream;

        // don't try to reconnect. On an error in the output socket
        // writes will cease, but this process will keep reading samples.
        output.setReconnectDelaySecs(-1);
        syncGen.connect(&output);
    }
    else
    {
        syncGen.addSampleClient(_sampleClient);
    }
}


void
SyncServer::
read(bool once) throw(n_u::IOException)
{
    bool eof = false;
    RawSampleInputStream& sis = *_inputStream;
    try {
        while (!once)
        {
            if (isInterrupted()) break;
            sis.readSamples();
        }
    }
    catch (n_u::EOFException& xceof)
    {
        eof = true;
        cerr << xceof.what() << endl;
    }
    catch (n_u::IOException& ioe)
    {
        stop();
        throw(ioe);
    }
    // In the normal eof case, flush the sorter pipeline.
    if (eof)
    {
        pipeline.flush();
        stop();
    }
}


