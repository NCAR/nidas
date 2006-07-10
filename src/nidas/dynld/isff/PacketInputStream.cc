/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-23 12:30:55 -0600 (Tue, 23 May 2006) $

    $LastChangedRevision: 3364 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/dynld/SampleInputStream.cc $
 ********************************************************************
*/

#include <nidas/core/Project.h>
#include <nidas/dynld/isff/PacketInputStream.h>
#include <nidas/dynld/isff/GOESOutput.h>
#include <nidas/core/DSMService.h>

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,PacketInputStream)

/*
 * Constructor, with a IOChannel (which may be null).
 */
PacketInputStream::PacketInputStream(IOChannel* iochannel)
    throw(n_u::InvalidParameterException):
    iochan(iochannel),iostream(0),packetParser(0)
{
    if (iochan)
        iostream = new IOStream(*iochan,iochan->getBufferSize());
    collectSampleTags();
}

/*
 * Copy constructor, with a new IOChannel.
 */
PacketInputStream::PacketInputStream(const PacketInputStream& x,
	IOChannel* iochannel):
    iochan(iochannel),iostream(0),packetParser(0)
{
    if (iochan)
        iostream = new IOStream(*iochan,iochan->getBufferSize());
    try {
	collectSampleTags();
    }
    catch(const n_u::InvalidParameterException & e) {
	n_u::Logger::getInstance()->log(LOG_ERR,
		"PacketInputStream: %s",e.what());
    }
}

/*
 * Clone myself, with a new IOChannel.
 */
PacketInputStream* PacketInputStream::clone(IOChannel* iochannel)
{
    return new PacketInputStream(*this,iochannel);
}

PacketInputStream::~PacketInputStream()
{
    delete iostream;
    delete iochan;
    delete packetParser;
    map<int,GOESProject*>::const_iterator pi =
    	projectsByConfigId.begin();
    for ( ; pi != projectsByConfigId.end(); ++pi) delete pi->second;
    for (unsigned int i = 0; i < goesTags.size(); i++) delete goesTags[i];
}

string PacketInputStream::getName() const
{
    if (iochan) return string("PacketInputStream: ") + iochan->getName();
    return string("PacketInputStream");
}

const set<const SampleTag*>& PacketInputStream::getSampleTags() const
{
    return sampleTags;
}

void PacketInputStream::collectSampleTags()
    throw(n_u::InvalidParameterException)
{
    if (sampleTags.size() > 0) return;

    GOESProject* gp = 0;
    if (projectsByConfigId.size() == 0) {
	Project* project = Project::getInstance();
	const Parameter* cfg = project->getParameter("goes_config");
	if (!cfg)
	    throw n_u::InvalidParameterException(
		project->getName(),"goes_config","not found");

	if (cfg->getType() != Parameter::INT_PARAM)
	    throw n_u::InvalidParameterException(
		project->getName(),"goes_config","not an integer");
	const ParameterT<int>* icfg = static_cast<const ParameterT<int>*>(cfg);
	if (icfg->getLength() != 1)
	    throw n_u::InvalidParameterException(
		project->getName(),"goes_config","not length 1");
    	int cid = icfg->getValue(0);
	gp = new GOESProject(project);
	projectsByConfigId[cid] = gp;
    }

    sampleTags = gp->getSampleTags();

    dsm_sample_id_t maxSampleId = 0;
    int maxStation = 0;

    set<const SampleTag*>::const_iterator ti = sampleTags.begin();
    for (; ti != sampleTags.end(); ++ti) {
        const SampleTag* tag = *ti;
	int stationNumber = tag->getStation();
	int xmitOffset = gp->getXmitOffset(stationNumber);
	xmitOffsetsByTag[tag] = xmitOffset;

	maxSampleId = std::max(maxSampleId,tag->getShortId());
	maxStation = std::max(maxStation,tag->getStation());
    }

    const char* vars[][3] = {
	{"ClockError.GOES",	"sec",
	    "Actual-expected packet receipt time"},
	{"Signal.GOES",	"dBm",
	    "Received GOES signal strength"},
	{"FreqOffset.GOES",	"Hz",
	    "Offset from assigned GOES center frequency"},
	{"Channel.GOES",	" ",
	    "GOES Channel Number (neg=West, pos=East)"},
	{"MsgStatus.GOES",	" ",
	    "0=GOOD"},
    };

    // cerr << "maxStation=" << maxStation << endl;
    // cerr << "nvars=" << sizeof(vars)/sizeof(vars[0]) << endl;
    for (int i = 0; i <= maxStation; i++) {
	SampleTag* goesTag = new SampleTag();
	for (unsigned int j = 0; j < sizeof(vars)/sizeof(vars[0]); j++) {
	    Variable* var = new Variable();
	    var->setName(vars[j][0]);
	    var->setUnits(vars[j][1]);
	    var->setLongName(vars[j][2]);
	    goesTag->addVariable(var);
	}
	goesTag->setSampleId(maxSampleId+1);
	goesTag->setDSMId(i+1);
	goesTag->setStation(i);
	goesTags.push_back(goesTag);
	sampleTags.insert(goesTag);
    }
}

void PacketInputStream::init() throw()
{
    if (!iostream)
	iostream = new IOStream(*iochan,iochan->getBufferSize());

    // throws ParseException if internal regular expressions don't
    // compile - programmer error.
    try {
	if (!packetParser) packetParser = new PacketParser();
    }
    catch (const n_u::ParseException& e) {
	n_u::Logger::getInstance()->log(LOG_WARNING,
		"Programmer error: %s",e.what());
	throw e;
    }


}

void PacketInputStream::close() throw(n_u::IOException)
{
    delete iostream;
    iostream = 0;
    delete packetParser;
    packetParser = 0;
    iochan->close();
}

void PacketInputStream::readSamples() throw(n_u::IOException)
{
    char packet[1024];
    size_t len = iostream->readUntil(packet,sizeof(packet),'\n');

    if (packet[len-1] != '\n')
    	throw n_u::IOException(getName(),"readUntil",
		"no termination character found");

    PacketParser::packet_type ptype;

    try {
	ptype = packetParser->parse(packet);
    }
    catch (const n_u::ParseException& e) {
	n_u::Logger::getInstance()->log(LOG_WARNING,
	    "%s: %s",getName().c_str(),e.what());
	return;
    }

#ifdef DEBUG
    cerr << hex << packetParser->getStationId() << dec << ' ' << 
    	packetParser->getPacketTime().format(true,"%c") << ' ' <<
	*packetParser->getPacketInfo() << endl;
#endif

    switch(ptype) {
    case PacketParser::NESDIS_PT:
        if (packetParser->getPacketInfo()->getStatusInt() & 0xf) return;
	break;
    default:
        return;
    }

    const SampleTag* tag;

    try {
	tag = findSampleTag(packetParser->getConfigId(),
	    packetParser->getStationId(),packetParser->getSampleId());
    }
    catch (const n_u::InvalidParameterException& e) {
        throw n_u::IOException(getName(),"readSamples",e.what());
    }

    if (!tag) return;

    int xmitOffset = getXmitOffset(tag);

    size_t nvars = tag->getVariables().size();
    // cerr << "nvars=" << nvars << endl;

    int stationNumber = tag->getStation();
    // cerr << "samp station number=" << stationNumber << endl;

    SampleT<float>* samp = getSample<float>(nvars);

    dsm_time_t tpack = packetParser->getPacketTime().toUsecs();
    int xmitPeriod = (int)rint(tag->getPeriod()) * USECS_PER_SEC;

    dsm_time_t texpect = tpack - (tpack % xmitPeriod);

    tpack -= xmitOffset * USECS_PER_SEC;

    int tdiff = (tpack - texpect) / USECS_PER_SEC;

    // cerr << "xmitPeriod=" << xmitPeriod << " tdiff=" << tdiff << endl;

    samp->setTimeTag(texpect - xmitPeriod / 2);
    samp->setId(tag->getId());

    packetParser->parseData(samp->getDataPtr(),nvars);

    distribute(samp);

    // send a sample of GOES info
    SampleTag* gtag = goesTags[stationNumber];

    samp = getSample<float>(gtag->getVariables().size());
    samp->setTimeTag(texpect - xmitPeriod / 2);
    samp->setId(gtag->getId());

    const PacketInfo* pinfo = packetParser->getPacketInfo();
    assert(samp->getDataLength() == 5);

    float* fptr = samp->getDataPtr();
    fptr[0] = tdiff;
    fptr[1] = pinfo->getSignalStrength();
    fptr[2] = pinfo->getFreqOffset();
    fptr[3] = pinfo->getChannel();
    fptr[4] = pinfo->getStatusInt();
    distribute(samp);
}

const SampleTag* PacketInputStream::findSampleTag(int configId,
	int goesId,int sampleId) throw(n_u::InvalidParameterException)
{
    GOESProject* gp = 0;

    if (projectsByConfigId.size() == 0) return 0;

    map<int,GOESProject*>::const_iterator pi =
    	projectsByConfigId.find(configId);

    if (pi == projectsByConfigId.end()) pi = projectsByConfigId.begin();
    
    gp = pi->second;

    int stationNumber = gp->getStationNumber(goesId);
    if (stationNumber < 0) return 0;

    const SampleTag* tag = gp->getSampleTag(stationNumber,sampleId);
    return tag;
}

int PacketInputStream::getXmitOffset(const SampleTag* tag) 
{
    return xmitOffsetsByTag[tag];
}

int PacketInputStream::GOESProject::getStationNumber(unsigned long goesId)
	throw(n_u::InvalidParameterException)
{
    if (stationNumbersById.size() == 0) readGOESIds();
    
    map<unsigned long,int>::const_iterator si = stationNumbersById.find(goesId);
    si = stationNumbersById.find(goesId);

    if (si != stationNumbersById.end()) return si->second;
    n_u::Logger::getInstance()->log(LOG_WARNING,
	"Can't find station id 0x%x, ignoring packet",goesId);
    return -1;
}

unsigned long PacketInputStream::GOESProject::getGOESId(int stationNum)
	throw(n_u::InvalidParameterException)
{
    if (stationNumbersById.size() == 0) readGOESIds();
    if (stationNum < 0 || stationNum >= (signed)goesIds.size()) return 0;
    return goesIds[stationNum];
}

void PacketInputStream::GOESProject::readGOESIds()
	throw(n_u::InvalidParameterException)
{
    const Parameter* ids = project->getParameter("goes_ids");

    if (!ids)
	throw n_u::InvalidParameterException(
	    project->getName(),"goes_ids","not found");

    if (ids->getType() != Parameter::INT_PARAM)
	throw n_u::InvalidParameterException(
	    project->getName(),"goes_ids","not an integer");

    const ParameterT<int>* iids = static_cast<const ParameterT<int>*>(ids);
    for (int i = 0; i < iids->getLength(); i++) {
	unsigned long goesId = (unsigned) iids->getValue(i);
	goesIds.push_back(goesId);
	stationNumbersById[goesId] = i;
    }
}

int PacketInputStream::GOESProject::getXmitOffset(int stationNumber)
	throw(n_u::InvalidParameterException)
{
    if ((signed) xmitOffsets.size() > stationNumber)
    	return xmitOffsets[stationNumber];

    if (xmitOffsets.size() > 0) {	// we've already fetched the offsets
	n_u::Logger::getInstance()->log(LOG_WARNING,
	    "Can't find goes_xmitOffsest for station %d",stationNumber);
	return -1;
    }

    const Parameter* ids = project->getParameter("goes_xmitOffsets");

    if (!ids)
	throw n_u::InvalidParameterException(
	    project->getName(),"goes_xmitOffsets","not found");

    if (ids->getType() != Parameter::INT_PARAM)
	throw n_u::InvalidParameterException(
	    project->getName(),"goes_xmitOffsets","not an integer");

    const ParameterT<int>* iids = static_cast<const ParameterT<int>*>(ids);
    xmitOffsets.clear();
    for (int i = 0; i < iids->getLength(); i++)
	xmitOffsets.push_back(iids->getValue(i));

    if (stationNumber >= iids->getLength()) {
	n_u::Logger::getInstance()->log(LOG_WARNING,
	    "Can't find goes_xmitOffsest for station %d",stationNumber);
	return -1;
    }
    return xmitOffsets[stationNumber];
}

const SampleTag* PacketInputStream::GOESProject::getSampleTag(
	int stationNumber, int sampleId)
{
    dsm_sample_id_t fullSampleId = sampleId;
    fullSampleId = SET_DSM_ID(fullSampleId,stationNumber+1);

    map<dsm_sample_id_t,SampleTag*>::const_iterator ti =
    	sampleTagsById.find(fullSampleId);

    if (ti != sampleTagsById.end()) return ti->second;
    return 0;
}

PacketInputStream::GOESProject::~GOESProject()
{
}
const set<const SampleTag*>& PacketInputStream::GOESProject::getSampleTags()
{
    if (sampleTags.size() > 0) return sampleTags;

    SiteIterator si = project->getSiteIterator();

    for ( ; si.hasNext(); ) {
        Site* site = si.next();

	ProcessorIterator pi = site->getProcessorIterator();
	for ( ; pi.hasNext(); ) {
	    SampleIOProcessor* proc = pi.next();
	    const list<SampleOutput*>& outputs = proc->getOutputs();
	    list<SampleOutput*>::const_iterator oi = outputs.begin();
	    for ( ; oi != outputs.end(); ++oi) {
		SampleOutput* output = *oi;
		GOESOutput* goesOutput =
		    dynamic_cast<nidas::dynld::isff::GOESOutput*>(output);
		if (goesOutput) {
		    const list<SampleTag*> tags =
			    goesOutput->getOutputSampleTags();
		    list<SampleTag*>::const_iterator ti = tags.begin();
		    for (; ti != tags.end(); ++ti) {
			SampleTag* tag = *ti;
			tag->setStation(site->getNumber());
			tag->setDSMId(site->getNumber()+1);
			sampleTags.insert(tag);
			sampleTagsById[tag->getId()] = tag;
			// Copy units and long name attributes
			// from the sensor variables.
			for (unsigned int iv = 0;
				iv < tag->getVariables().size(); iv++) {
			    Variable& v1 = tag->getVariable(iv);
			    if (v1.getUnits().length() == 0) {
				VariableIterator vi2 =
					project->getVariableIterator();
				for ( ; vi2.hasNext(); ) {
				    const Variable* v2 = vi2.next();
				    if (*v2 == v1) {
				        if (v2->getUnits().length() > 0)
					    v1.setUnits(v2->getUnits());
				        if (v2->getLongName().length() > 0)
					    v1.setLongName(v2->getLongName());
					break;
				    }
				}
			    }
			}
		    }
		}
	    }
	}
    }
    return sampleTags;
}

