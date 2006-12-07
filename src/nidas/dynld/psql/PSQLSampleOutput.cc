/* -*- mode: c++; c-basic-offset: 4; -*-
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include "PSQLSampleOutput.h"
#include "PSQLChannel.h"
#include <nidas/core/Project.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMSensor.h>

#include <nidas/util/InvalidParameterException.h>

#include <time.h>

#include "psql_impl.h"

NIDAS_CREATOR_FUNCTION_NS(psql,PSQLSampleOutput)

PSQLSampleOutput::PSQLSampleOutput(): 
    connectionRequester(0),service(0),psqlChannel(0),
    missingValue(1.e37),first(true),dberrors(0)
{
}

PSQLSampleOutput::PSQLSampleOutput(const PSQLSampleOutput& x): 
	name(x.name),connectionRequester(0),
	dsms(x.dsms),service(x.service),psqlChannel(0),
	missingValue(x.missingValue),first(true),dberrors(0)
{
}

PSQLSampleOutput::~PSQLSampleOutput()
{
    delete psqlChannel;
}

PSQLSampleOutput* PSQLSampleOutput::clone(IOChannel* iochannel) const
{
    LOG (LOG_DEBUG, "PSQLSampleOutput: cloning %s", name.c_str());
    PSQLSampleOutput* out = new PSQLSampleOutput(*this);
    out->psqlChannel = static_cast<PSQLChannel*>(iochannel);
    return out;
}

void PSQLSampleOutput::requestConnection(SampleConnectionRequester* requester)
        throw(nidas::util::IOException)
{
    ENTER;
    LOG(LOG_DEBUG, "connection requested of %s", name.c_str());
    connectionRequester = requester;
    psqlChannel->requestConnection(this);
}

void
PSQLSampleOutput::
connect() throw(nidas::util::IOException)
{
    ENTER;
    LOG(LOG_DEBUG, "connection requested of %s", name.c_str());
    // Where does the new IOChannel returned by this method go?
    psqlChannel->connect();
}

void PSQLSampleOutput::setDSMConfigs(const std::list<const DSMConfig*>& val)
{
    dsms = val;
}

void PSQLSampleOutput::addDSMConfig(const DSMConfig* val)
{
    dsms.push_back(val);
}

const std::list<const DSMConfig*>& PSQLSampleOutput::getDSMConfigs() const
{
    return dsms;
}


void 
PSQLSampleOutput::
connected(SampleOutput* origout, SampleOutput* newout) throw()
{
    ENTER;
    LOG(LOG_DEBUG, "%s connected()", name.c_str());
    connectionRequester->connected(origout, newout);
}


void
PSQLSampleOutput::
connected(IOChannel* output) throw()
{
    // The PSQLChannel is telling us the connection succeeded.
    ENTER;
    LOG(LOG_DEBUG, "connected(%s)", output->getName().c_str());
    SampleOutputStream::connected(output);
}


void PSQLSampleOutput::flush() throw(nidas::util::IOException)
{
    if (psqlChannel) psqlChannel->flush();
}

void
PSQLSampleOutput::
close() throw(nidas::util::IOException)
{
    if (psqlChannel) psqlChannel->close();
}

void
PSQLSampleOutput::
addSampleTag(const SampleTag* tag)
    throw(nidas::util::InvalidParameterException)
{
    ENTER;
    map<float,const SampleTag*>::const_iterator ti;

    if ((ti = tagsByRate.find(tag->getRate())) != tagsByRate.end()) {
	ostringstream errst;
	errst << tag->getRate();
    	throw nidas::util::InvalidParameterException(getName(),"addSampleTag",
		"duplicate sample rate:" + errst.str());
    }
    tagsByRate[tag->getRate()] = tag;
    tagsById[tag->getId()] = tag;
    if (tag->getRate() == 1.0)
	tablesByRate[tag->getRate()] = "RAF_LRT";
    else {
	ostringstream costr;
	costr << "SampleRate" << tag->getRate();
	tablesByRate[tag->getRate()] = costr.str();
    }

}

void
PSQLSampleOutput::
init() throw()
{
    ENTER;
    try {
	dropAllTables();      // Remove existing tables, this is a reset.
	createTables();
	initializeGlobalAttributes();
    }
    catch(const nidas::util::IOException& ioe) {
	nidas::util::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		getName().c_str(),ioe.what());
    }
}

void PSQLSampleOutput::submitCommand(const string& command)
	throw(nidas::util::IOException)
{
    psqlChannel->write(command.c_str(),command.length()+1);
}

void PSQLSampleOutput::createTables() throw(nidas::util::IOException)
{
    submitCommand(
      "CREATE TABLE Global_Attributes (key char(20) PRIMARY KEY, value char(120))");                                                                                
    submitCommand("\
	CREATE TABLE Variable_List (Name char(20) PRIMARY KEY, \
	Units char(16), Uncalibrated_Units char(16), long_name char(80), \
	SampleRateTable char(16), nDims int,dims int[], nCals int, \
	poly_cals float[], missing_value float, data_quality char(16))");
                                                                                
    submitCommand("\
	CREATE TABLE Categories (variable char(20), category char(20))");

    map<float,const SampleTag*>::const_iterator ti;
    cerr << "tagsByRate.size=" << tagsByRate.size() << endl;
    for (ti = tagsByRate.begin(); ti != tagsByRate.end(); ++ti) {
        float rate = ti->first;
	const SampleTag* tag = ti->second;

	ostringstream costr;
	if (rate == 1.0)
	    costr << "CREATE Table RAF_LRT (datetime timestamp PRIMARY KEY,";
	else
	    costr << "CREATE Table SampleRate" << rate << " (datetime timestamp (3) PRIMARY KEY,";

	const vector<const Variable*>& vars = tag->getVariables();

	vector<const Variable*>::const_iterator vi;

	char comma = ' ';
	for (vi = vars.begin(); vi != vars.end(); ++vi) {
	    const Variable* var = *vi;
	    if (!var->getName().compare("Clock")) continue;
	    addVariable(var);	// adds to Variable_List and Categories
	    costr << comma << var->getName()  << " FLOAT";
	    comma = ',';
	}
	costr << ")";
	submitCommand(costr.str());
    }
}

void
PSQLSampleOutput::
dropAllTables() throw()
{
    ENTER;
    try {
	for (;;) submitCommand("DROP TABLE Global_Attributes");
    }
    catch(const nidas::util::IOException& ioe) {
	nidas::util::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		getName().c_str(),ioe.what());
    }
    try {
	for(;;) submitCommand("DROP TABLE Variable_List");
    }
    catch(const nidas::util::IOException& ioe) {
	nidas::util::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		getName().c_str(),ioe.what());
    }
    try {
	for(;;) submitCommand("DROP TABLE Categories");
    }
    catch(const nidas::util::IOException& ioe) {
	nidas::util::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		getName().c_str(),ioe.what());
    }
    try {
	for(;;) submitCommand("DROP TABLE RAF_LRT");
    }
    catch(const nidas::util::IOException& ioe) {
	nidas::util::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		getName().c_str(),ioe.what());
    }

    try {
	submitCommand("VACUUM FULL");
    }
    catch(const nidas::util::IOException& ioe) {
	nidas::util::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		getName().c_str(),ioe.what());
    }

}

/* -------------------------------------------------------------------- */
void
PSQLSampleOutput::
initializeGlobalAttributes() throw(nidas::util::IOException)
{
    ENTER;
    // Add Global Attributes/Flight Data.
    submitCommand(
	"INSERT INTO global_attributes VALUES ('Source', 'NCAR Research Aviation Facility')");
    submitCommand(
      "INSERT INTO global_attributes VALUES ('Address', 'P.O. Box 3000, Boulder, CO 80307-3000')");
    submitCommand(
      "INSERT INTO global_attributes VALUES ('Phone', '(303) 497-1030')");
                                                                                
    ostringstream costr;
    costr << "INSERT INTO global_attributes VALUES ('ProjectName','" <<
      Project::getInstance()->getName() << "')";
    submitCommand(costr.str());

    costr.str("");
    costr << "INSERT INTO global_attributes VALUES ('ProjectNumber', '" <<
      // Project::getInstance()->getNumber() <<
      0 <<
      "')";
    submitCommand(costr.str());

    costr.str("");
    costr << "INSERT INTO global_attributes VALUES ('FlightNumber', '" <<
	0 << "')";
    submitCommand(costr.str());

    // real-time
    submitCommand(
	"CREATE RULE update AS ON UPDATE TO global_attributes DO NOTIFY current");

    costr.str("");
    time_t tnow = time(0);
    struct tm tm;
    localtime_r(&tnow,&tm);
    char date[64];
    strftime(date,sizeof(date),"%Y %b %d %H:%M:%S %z",&tm);
    costr << "INSERT INTO global_attributes VALUES ('DateProcessed', '" <<
	date << "')";
    submitCommand(costr.str());
}


void
PSQLSampleOutput::
addVariable(const Variable* var)
    throw(nidas::util::IOException)
{
    if (!var->getName().compare("Clock")) return;
    string calibratedUnits;
    const VariableConverter* converter = var->getConverter();
    if (converter) calibratedUnits = converter->getUnits();
    else calibratedUnits = var->getUnits();

    ostringstream costr;
    costr << "INSERT INTO Variable_List VALUES ('" <<
        var->getName()            << "', '" <<
        calibratedUnits           << "', '" <<
        var->getUnits() 	  << "', '" <<
        var->getLongName()        << "', '";

    float sampleRate = var->getSampleTag()->getRate();
    if (sampleRate > 0) costr << "SampleRate " << sampleRate;

    int nDims = 1;
    int dims[] = { 1 };
    int nCals = 2;
    float cal[] = { 0.0, 1.0 };

    costr << "', '"  << nDims <<  "', '{";
    for (int i = 0; i < nDims; ++i) {
	if (i > 0) costr << ',';
	costr << dims[i];
    }
    costr << "}', '" << nCals << "', '{";

    for (int i = 0; i < nCals; ++i) {
	if (i > 0) costr << ',';
	costr << cal[i];
    }
    string dataQuality = "Preliminary";
    costr << "}', '" << missingValue << "', '" << dataQuality << "')";
    submitCommand(costr.str());
 
    // see /jnet/shared/proj/defaults/Categories
    addCategory(var->getName(), "Raw");
}

void PSQLSampleOutput::addCategory(const string& varName,const string& category)
	throw(nidas::util::IOException)
{
    if (category.length() == 0) return;

    string cmd = "INSERT INTO categories VALUES ('" + varName +
                        "', '" + category + "')";
    submitCommand(cmd);
}

bool PSQLSampleOutput::receive(const Sample* samp) throw()
{
    
    if (samp->getType() != FLOAT_ST) {
	cerr << "sample=" << samp->getId() << " not FLOAT" << endl;
        return false;
    }

    const SampleT<float>* fsamp = (const SampleT<float>*) samp;

    struct tm tm;
    time_t tt = (time_t)(samp->getTimeTag() / USECS_PER_SEC);
    gmtime_r(&tt,&tm);
    char tstr[32];
    strftime(tstr,sizeof(tstr),"%Y-%m-%d %H:%M:%S",&tm);
    sprintf(tstr+strlen(tstr),".%03d",
    	(int)(samp->getTimeTag() % USECS_PER_SEC) / USECS_PER_MSEC);

    ostringstream costr;
    if (first) {
	costr << "INSERT INTO global_attributes VALUES ('StartTime', '" <<
		 tstr << "')";
	submitCommand(costr.str());
	cerr << "submitCommand, costr=" << costr.str() << endl;
	costr.str("");

	costr << "INSERT INTO global_attributes VALUES ('EndTime', '" <<
		 tstr << "')";
	submitCommand(costr.str());
	cerr << "submitCommand, costr=" << costr.str() << endl;
	costr.str("");
	first = false;
    }

    map<dsm_sample_id_t,const SampleTag*>::const_iterator ti;
    ti = tagsById.find(samp->getId());
    if (ti == tagsById.end()) {
	cerr << "tag not found: " << samp->getId() <<
		" tags.size=" << tagsById.size() << endl;
        return false;
    }
    const SampleTag* tag = ti->second;

    map<float,string>::const_iterator tabi;
    tabi = tablesByRate.find(tag->getRate());
    assert(tabi != tablesByRate.end());
    const string& table = tabi->second;

    costr << "INSERT INTO " << table << " VALUES ('" << tstr << "',";

    char comma = ' ';

    const float* fptr = fsamp->getConstDataPtr();

    for (size_t i = 0; i < fsamp->getDataLength(); i++) {
	float value = fptr[i];
	if (isnan(value) || isinf(value)) costr << comma << missingValue;
	else costr << comma << value;
	comma = ',';
    }
    costr << ");";

    try {
	cerr << "submitCommand, costr=" << costr.str() << endl;
        submitCommand(costr.str());
    }
    catch (const nidas::util::IOException& ioe) {
        dberrors++;
	nidas::util::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		getName().c_str(),ioe.what());
    }

    costr.str("");
    costr << "UPDATE global_attributes SET value='"
        << tstr << "' WHERE key='EndTime';";
    try {
	submitCommand(costr.str());
    }
    catch (const nidas::util::IOException& ioe) {
        dberrors++;
	nidas::util::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		getName().c_str(),ioe.what());
    }
    return true;
}

void PSQLSampleOutput::fromDOMElement(const DOMElement* node)
        throw(nidas::util::InvalidParameterException)
{
    ENTER;
    XDOMElement xnode(node);
    //    const string& elname = xnode.getNodeName();
    if(node->hasAttributes()) {
    // get all the attributes of the node
        DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((DOMAttr*) pAttributes->item(i));
            // get attribute name
	    //            const std::string& aname = attr.getName();
	    //            const std::string& aval = attr.getValue();
        }
    }

    // process <postgresdb> (should only be one)

    int niochan = 0;
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;

	IOChannel* iochan = IOChannel::createIOChannel((DOMElement*)child);

	if (!(psqlChannel = dynamic_cast<PSQLChannel*>(iochan))) {
	    delete iochan;
            throw nidas::util::InvalidParameterException(
                    "PSQLSampleOutput::fromDOMElement",
                    "output", "must be a PSQLChannel");
	}

	//        psqlChannel->setDSMConfigs(getDSMConfigs());
        psqlChannel->fromDOMElement((DOMElement*)child);

        if (++niochan > 1)
            throw nidas::util::InvalidParameterException(
                    "SampleOutputStream::fromDOMElement",
                    "output", "must have one child element");
    }
    if (!psqlChannel)
        throw nidas::util::InvalidParameterException(
                "SampleOutputStream::fromDOMElement",
                "output", "must have one child element");
    setName(psqlChannel->getName());
}

DOMElement* PSQLSampleOutput::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
                        DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}
                                                                                
DOMElement* PSQLSampleOutput::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

