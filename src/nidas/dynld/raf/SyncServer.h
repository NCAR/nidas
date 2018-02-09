// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_RAF_SYNCSERVER_H
#define NIDAS_DYNLD_RAF_SYNCSERVER_H

#include <nidas/core/Socket.h>
#include "SyncRecordGenerator.h"
#include <nidas/core/SamplePipeline.h>
#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/dynld/SampleOutputStream.h>
#include <nidas/util/Thread.h>
#include <nidas/util/auto_ptr.h>

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


class SyncServer : public nidas::util::Thread,
                   public nidas::core::SampleConnectionRequester
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

    void
    read(bool once = false) throw(nidas::util::IOException);

    void
    setSorterLengthSeconds(float sorter_secs)
    {
        _sorterLengthSecs = sorter_secs;
    }

    void
    setRawSorterLengthSeconds(float sorter_secs)
    {
        _rawSorterLengthSecs = sorter_secs;
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

    void
    getTimeWindow(nidas::util::UTime* start, nidas::util::UTime* end);

    void
    setTimeWindow(nidas::util::UTime start, nidas::util::UTime end);

    static const int DEFAULT_PORT = 30001;

    static const float SORTER_LENGTH_SECS;

    static const float RAW_SORTER_LENGTH_SECS;

    /**
     * Implementation of SampleConnectionRequester::connect().
     * Does nothing.
     */
    void connect(SampleOutput* output) throw();

    /**
     * Implementation of SampleConnectionRequester::disconnect().
     * If client has disconnected, interrupt the sample loop
     * and exit.
     */
    void disconnect(SampleOutput* output) throw();

private:

    void
    initProject();

    void
    initSensors(SampleInputStream& sis);

    void
    stop();

    void
    signalStop();

    void
    handleSample(nidas::core::Sample* sample);

    SamplePipeline _pipeline;
    SyncRecordGenerator _syncGen;

    RawSampleInputStream* _inputStream;
    SampleOutputStream* _outputStream;

    std::string _xmlFileName;

    std::list<std::string> _dataFileNames;

    nidas::util::auto_ptr<nidas::util::SocketAddress> _address;

    float _sorterLengthSecs;

    float _rawSorterLengthSecs;

    SampleClient* _sampleClient;

    StopSignal* _stop_signal;

    nidas::core::Sample* _firstSample;

    dsm_time_t _startTime;

    // Skip samples outside the time window.
    dsm_time_t _startWindow;
    dsm_time_t _endWindow;

    SyncServer(const SyncServer&);

    SyncServer&
    operator=(const SyncServer&);

};

}}}	// namespace nidas namespace dynld namespace raf


#endif // NIDAS_DYNLD_RAF_SYNCSERVER_H


