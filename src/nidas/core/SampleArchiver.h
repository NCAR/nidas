
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_SAMPLEARCHIVER_H
#define NIDAS_CORE_SAMPLEARCHIVER_H

#include <nidas/core/SampleIOProcessor.h>
#include <nidas/core/SampleSorter.h>
#include <nidas/core/FileSet.h>
#include <nidas/util/ThreadSupport.h>

namespace nidas { namespace core {

class SampleArchiver: public SampleIOProcessor
{
public:
    
    SampleArchiver();

    virtual ~SampleArchiver();

    /**
     * By default a SampleArchiver is used for archiving raw samples,
     * and any SampleOutputs that connect will receive samples
     * from source->getRawSampleSource(), even if the output->isRaw()
     * method returns false.  This is to correct possible human errors
     * in the configuation.
     * If setRaw(false), then this SampleArchiver will archive 
     * processed samples, by connecting outputs to
     * source->getProcessedSampleSource().
     * If the value of an output->isRaw() disgrees with getRaw() of
     * this SampleArchive, a warning message is logged.
     */
    void setRaw(bool val) 
    {
        _rawArchive = val;
    }

    bool getRaw() const
    {
        return _rawArchive;
    }

    /**
     * Implementation of SampleIOProcessor::connect(SampleSource*).
     */
    void connect(SampleSource*) throw();

    /**
     * Implementation of SampleIOProcessor::disconnect(SampleSource*).
     */
    void disconnect(SampleSource*) throw();

    /**
     * Implementation of SampleConnectionRequester::connect(SampleOutput*).
     */
    void connect(SampleOutput* output) throw();

    /**
     * Implementation of SampleConnectionRequester::disconnect(SampleOutput*).
     */
    void disconnect(SampleOutput* output) throw();

    void printStatus(std::ostream&,float deltat,int&) throw();

private:

    std::string _lastFileSetState;

    int _lastZebra;

    nidas::util::Mutex _connectionMutex;

    std::set<SampleSource*> _connectedSources;

    std::set<SampleOutput*> _connectedOutputs;

    /**
     * If my SampleOutput* is a nidas::core::FileSet then save the pointer
     * for use in by printStatus(), so that the status output will contain
     * things like the file size.
     */
    std::list<const nidas::core::FileSet*> _filesets;

    /**
     * Mutex for controlling access to _filesets
     * so that printStatus has valid pointers.
     */
    nidas::util::Mutex _filesetMutex;

    /**
     * Saved between calls to printStatus in order to compute sample rates.
     */
    size_t _nsampsLast;

    /**
     * Saved between calls to printStatus in order to compute data rates.
     */
    long long _nbytesLast;

    std::map<const nidas::core::FileSet*,long long> _nbytesLastByFileSet;

    bool _rawArchive;

    /**
     * Copy not supported.
     */
    SampleArchiver(const SampleArchiver& x);

    /**
     * Assignment not supported.
     */
    SampleArchiver& operator=(const SampleArchiver& x);


};

}}	// namespace nidas namespace core

#endif
