
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-06-01 15:27:18 -0600 (Wed, 01 Jun 2005) $

    $LastChangedRevision: 2169 $

    $LastChangedBy: maclean $

    $HeadURL: http://localhost:8080/svn/hiaper/ads3/dsm/class/SyncRecordProcessor.h $
 ********************************************************************

*/

#ifndef DSM_SYNCRECORDREADER_H
#define DSM_SYNCRECORDREADER_H

#include <SampleInput.h>
#include <SampleTag.h>
#include <SyncRecordVariable.h>

#include <atdUtil/ThreadSupport.h>

#include <semaphore.h>

namespace dsm {

class SyncRecHeaderException: public atdUtil::Exception 
{
public:
    SyncRecHeaderException(const std::string& expect, const std::string& got):
    	atdUtil::Exception("SyncRecHeaderException",
		std::string("expected: \"") + expect +
		"\", got: \"" + got + "\"")
    {
    }
    SyncRecHeaderException(const std::string& msg):
    	atdUtil::Exception("SyncRecHeaderException",msg)
    {
    }
};

class SyncRecordReader: public SampleClient
{
public:
    SyncRecordReader();

    virtual ~SyncRecordReader();

    /**
     * Connect a SampleInput.
     */
    void connect(SampleInput*) throw(atdUtil::IOException);

    /**
     * Disconnect a SampleInput.
     */
    void disconnect(SampleInput*) throw(atdUtil::IOException);

    const std::list<const SyncRecordVariable*> getVariables()
    	throw(atdUtil::Exception);

    size_t read(float *ptr,size_t len) throw();

    bool receive(const Sample* samp) throw();

private:

    void scanHeader(const Sample* samp) throw();

    std::string getQuotedString(std::istringstream& str);
    
    sem_t varsSem;

    atdUtil::Mutex varsMutex;

    SyncRecHeaderException* exception;

    std::list<SampleTag*> sampleTags;

    std::list<const SyncRecordVariable*> variables;

    sem_t readSem;

    atdUtil::Mutex recsMutex;

    std::list<const Sample*> syncRecs;


};


}

#endif
