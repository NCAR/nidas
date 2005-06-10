
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef DSM_PSQLSAMPLEOUTPUT_H
#define DSM_PSQLSAMPLEOUTPUT_H

#include <SampleOutput.h>
#include <SampleTag.h>

#include <map>

namespace dsm {

class PSQLSampleOutput : public SampleOutput
{
public:

    PSQLSampleOutput();

    PSQLSampleOutput(const PSQLSampleOutput&);

    virtual ~PSQLSampleOutput();

    SampleOutput* clone(IOChannel* iochannel = 0) const;

    void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

    bool isRaw() const { return false; }

    void setPseudoPort(int val) {}

    int getPseudoPort() const { return 0; }

    void setDSMConfigs(const std::list<const DSMConfig*>& val);

    void addDSMConfig(const DSMConfig*);

    const std::list<const DSMConfig*>& getDSMConfigs() const;

    // void setDSMService(const DSMService* val) { service = val; }

    // const DSMService* getDSMService() const { return service; }

    void addSampleTag(const SampleTag* tag)
    	throw(atdUtil::InvalidParameterException);

    void requestConnection(SampleConnectionRequester*)
        throw(atdUtil::IOException);

    void connected(IOChannel* sock) throw();

    void init() throw(atdUtil::IOException);

    bool receive(const Sample*) throw();

    int getFd() const { return -1; }

    void flush() throw(atdUtil::IOException);

    void close() throw(atdUtil::IOException);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);
                                                                                
    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);

protected:

    void submitCommand(const std::string& command) throw(atdUtil::IOException);

    void createTables() throw(atdUtil::IOException);

    void dropAllTables() throw(atdUtil::IOException);

    void initializeGlobalAttributes() throw(atdUtil::IOException);

    void addVariable(const Variable* var) throw(atdUtil::IOException);

    void addCategory(const std::string& varName, const std::string& category)
    	throw(atdUtil::IOException);

    std::string name;

    SampleConnectionRequester* connectionRequester;

    std::list<const DSMConfig*> dsms;

    const DSMService* service;

    IOChannel* psqlChannel;

    // std::list<const SampleTag*> sampleTags;

    std::map<float,const SampleTag*> tagsByRate;

    std::map<float,std::string> tablesByRate;

    std::map<dsm_sample_id_t,const SampleTag*> tagsById;

    float missingValue;

    bool first;

    int dberrors;
};

}

#endif

