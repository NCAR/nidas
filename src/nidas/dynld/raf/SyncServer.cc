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

#include <nidas/dynld/raf/SyncRecordGenerator.h>
#include <nidas/core/FileSet.h>
#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/dynld/SampleOutputStream.h>
#include <nidas/core/SampleOutputRequestThread.h>
#include <nidas/core/Project.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/SamplePipeline.h>
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
    _xmlFileName(), _dataFileNames(),
    _address(new n_u::Inet4SocketAddress(DEFAULT_PORT)),
    _sorterLengthSecs(SORTER_LENGTH_SECS),
    _interrupted(false)
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

int SyncServer::run() throw(n_u::Exception)
{

    try {

        Project project;

        IOChannel* iochan = 0;

        nidas::core::FileSet* fset =
            nidas::core::FileSet::getFileSet(_dataFileNames);

        iochan = fset->connect();

        // RawSampleStream owns the iochan ptr.
        RawSampleInputStream sis(iochan);

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

        {
            auto_ptr<xercesc::DOMDocument> 
                doc(parseXMLConfigFile(_xmlFileName));
            project.fromDOMElement(doc->getDocumentElement());
        }

        XMLImplementation::terminate();

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

        SamplePipeline pipeline;
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

        SyncRecordGenerator syncGen;
	syncGen.connect(pipeline.getProcessedSampleSource());

	nidas::core::ServerSocket* servSock = new nidas::core::ServerSocket(*_address.get());
        IOChannel* ioc = servSock->connect();
        if (ioc != servSock) {
            servSock->close();
            delete servSock;
        }
        SampleOutputStream output(ioc,&syncGen);

        // don't try to reconnect. On an error in the output socket
        // writes will cease, but this process will keep reading samples.
        output.setReconnectDelaySecs(-1);
	syncGen.connect(&output);

        try {
            for (;;) {
                if (_interrupted) break;
                sis.readSamples();
            }
            sis.close();
            pipeline.flush();
            syncGen.disconnect(pipeline.getProcessedSampleSource());
            syncGen.disconnect(&output);
            output.close();
        }
        catch (n_u::EOFException& eof) {
            sis.close();
            pipeline.flush();
            syncGen.disconnect(pipeline.getProcessedSampleSource());
            syncGen.disconnect(&output);
            output.close();
            cerr << eof.what() << endl;
        }
        catch (n_u::IOException& ioe) {
            sis.close();
            pipeline.flush();
            syncGen.disconnect(pipeline.getProcessedSampleSource());
            syncGen.disconnect(&output);
            output.close();
            pipeline.interrupt();
            pipeline.join();
            throw(ioe);
        }
        pipeline.interrupt();
        pipeline.join();
    }
    catch (n_u::Exception& e) {
        cerr << e.what() << endl;
        XMLImplementation::terminate(); // ok to terminate() twice
	return 1;
    }
    SampleOutputRequestThread::destroyInstance();
    SamplePools::deleteInstance();
    return 0;
}


