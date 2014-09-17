// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2014-08-11 14:44:53 -0600 (Mon, 11 Aug 2014) $

    $LastChangedRevision: 7095 $

    $LastChangedBy: granger $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/raf/SyncRecordReader.h $
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_RAF_SYNCSERVER_H
#define NIDAS_DYNLD_RAF_SYNCSERVER_H

#include <list>
#include <string>
#include <memory>

#include <nidas/core/Socket.h>
#include <nidas/dynld/raf/SyncRecordGenerator.h>
#include <nidas/core/SamplePipeline.h>
#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/dynld/SampleOutputStream.h>
#include <nidas/util/Thread.h>

namespace nidas { namespace dynld { namespace raf {

struct StopSignal
{
    StopSignal()
    {}

    virtual void stop() = 0;

    virtual ~StopSignal()
    {};

private:
    StopSignal(const StopSignal&);
    StopSignal& operator=(const StopSignal&);
};


class SyncServer : public nidas::util::Thread
{
public:

    SyncServer();

    ~SyncServer();

    /**
     * Open the data file input stream and read the nidas header, but
     * do not parse the project.
     **/
    void
    openStream();

    /**
     * Call this method to parse the project, setup sample tags, preload
     * calibrations using the time of the first sample, setup sample
     * streams, and init the sensors.  It should be called before calling
     * start() on the thread or before calling run() directly.
     *
     * Between calling this method and entering run(), no samples have been
     * processed, so the calibrations can be replaced and will take effect
     * when processing starts.
     **/
    void
    init() throw(nidas::util::Exception);

    /**
     * This method implements the Runnable interface for Threads, but it
     * can also be called synchronously when run from main().
     **/
    int
    run() throw(nidas::util::Exception);

    virtual void interrupt();

    /**
     * Trigger sendHeader() on the SyncRecordSource, using the start time
     * retrieved from the first sample in the SampleInputStream.
     * SyncRecordReader calls this method when connecting to a SyncServer
     * instance, so the SyncRecordReader can receive the header immediately
     * without requiring any samples to be processed first.
     **/
    void
    sendHeader();

    void
    read(bool once = false) throw(nidas::util::IOException);

    void
    setSorterLengthSeconds(float sorter_secs)
    {
        _sorterLengthSecs = sorter_secs;
    }

    /**
     * Return the current XML filename setting.  If called after calling
     * openStream() and before setting it explicitly with setXMLFileName(),
     * then the return value is the XML filename from the stream header.
     **/
    std::string
    getXMLFileName()
    {
        return _xmlFileName;
    }

    void
    setXMLFileName(const std::string& name)
    {
        _xmlFileName = name;
    }        

    void
    resetAddress(nidas::util::SocketAddress* addr)
    {
        _address.reset(addr);
    }

    /**
     * Specify a SampleClient instance to receive the sync samples instead
     * of writing the sync samples to an output stream.
     **/
    void
    addSampleClient(SampleClient* client)
    {
        _sampleClient = client;
    }

    void
    setDataFileNames(const std::list<std::string>& dataFileNames)
    {
        _dataFileNames = dataFileNames;
    }

    /**
     * Set a callback function which will be called when the SyncServer
     * reaches EOF on its input stream or stops on an error.  Since the
     * pipeline is flushed when the input is closed, no more sync samples
     * will be sent after this function is called.  It is called from the
     * thread running the run() method.
     **/
    void
    setStopSignal(StopSignal *stop_signal)
    {
        _stop_signal = stop_signal;
    }

    static const int DEFAULT_PORT = 30001;

    static const float SORTER_LENGTH_SECS = 2.0;

private:

    void
    initProject();

    void
    initSensors(SampleInputStream& sis);

    void
    stop();

    SamplePipeline pipeline;
    SyncRecordGenerator syncGen;

    RawSampleInputStream* _inputStream;
    SampleOutputStream* _outputStream;

    std::string _xmlFileName;

    std::list<std::string> _dataFileNames;

    std::auto_ptr<nidas::util::SocketAddress> _address;

    float _sorterLengthSecs;

    SampleClient* _sampleClient;

    StopSignal* _stop_signal;

    nidas::core::Sample* _firstSample;

    dsm_time_t _startTime;

    SyncServer(const SyncServer&);

    SyncServer&
    operator=(const SyncServer&);

};

}}}	// namespace nidas namespace dynld namespace raf


#endif // NIDAS_DYNLD_RAF_SYNCSERVER_H


