// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_DSMSERVERAPP_H
#define NIDAS_CORE_DSMSERVERAPP_H

#include <nidas/core/Datasets.h>
#include <nidas/core/XMLException.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/IOException.h>
#include <nidas/util/InvalidParameterException.h>

#include <signal.h>

namespace nidas { namespace core {

class Project;
class DSMServer;
class DSMServerIntf;
class DSMServerStat;

class DSMServerApp {
public:

    DSMServerApp();

    ~DSMServerApp();

    /**
    * Run DSMServerApp from the unix command line.
    */
    static int main(int argc, char** argv) throw();

    static DSMServerApp* getInstance() { return _instance; }

    void initLogger();

    int initProcess(const char* argv0);

    int run() throw();

    int parseRunstring(int argc, char** argv);

    /**
     * Send usage message to cerr.
     */
    int usage(const char* argv0);

    /**
     * Invoke a XMLCachingParser to parse the XML and initialize the Project.
     */
    void parseXMLConfigFile(const std::string& xmlFileName,Project&)
        throw(nidas::core::XMLException,
            nidas::util::InvalidParameterException,nidas::util::IOException);

    void startXmlRpcThread() throw(nidas::util::Exception);

    void killXmlRpcThread() throw();

    void startStatusThread(DSMServer* svr) throw(nidas::util::Exception);

    void killStatusThread() throw();

    /**
     * What is the XML configuration file name.
     */
    const std::string& getXMLFileName() { return _xmlFileName; }

    std::string getUserName()
    { 
        return _username;
    }

    uid_t getUserID()
    {
        return _userid;
    }

    uid_t getGroupID()
    {
        return _groupid;
    }

    Dataset getDataset() throw(nidas::util::InvalidParameterException, XMLException);

private:

    /**
     * Create a signal mask, and block those masked signals.
     * DSMEngine uses sigwait, and does not register asynchronous
     * signal handlers.
     */
    void setupSignals();

    /**
     * Unblock and wait for signals of interest up to timeoutSecs.
     * After return, _runState will be set depending on 
     * the signal received.
     *
     */
    void waitForSignal(int timeoutSecs);

    static DSMServerApp* _instance;

    enum runState { RUN, QUIT, RESTART, ERROR };

    /**
     * -d option. If user wants messages on stderr rather than syslog.
     */
    bool _debug;

    /**
     * The xml file that is being used for configuration information.
     */
    std::string _xmlFileName;

    /**
     * The xml file that contains all the project configurations, by date.
     */
    std::string _configsXMLName;

    std::string _rafXML;

    std::string _isffXML;

    enum runState _runState;

    std::string _username;

    uid_t _userid;

    gid_t _groupid;

    /** This thread provides XML-based Remote Procedure calls */
    DSMServerIntf* _xmlrpcThread;

    DSMServerStat* _statusThread;

    bool _externalControl;

    int _logLevel;

    bool _optionalProcessing;

    sigset_t _signalMask;

    pthread_t _myThreadId;

    std::string _datasetName;

    /** Copy not needed */
    DSMServerApp(const DSMServerApp &);

    /** Assignment not needed */
    DSMServerApp& operator=(const DSMServerApp &);
};

}}	// namespace nidas namespace core

#endif
