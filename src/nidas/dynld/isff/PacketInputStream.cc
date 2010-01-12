/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

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
}

string PacketInputStream::getName() const
{
    if (iochan) return string("PacketInputStream: ") + iochan->getName();
    return string("PacketInputStream");
}

list<const SampleTag*> PacketInputStream::getSampleTags() const
{
    const GOESProject* gp = getGOESProject(0);
    return gp->getSampleTags();
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
	break;
    default:
        return;
    }

    dsm_time_t tpack = packetParser->getPacketTime().toUsecs();
    const PacketInfo* pinfo = packetParser->getPacketInfo();

    try {

	const GOESProject* gp = getGOESProject(packetParser->getConfigId());
	int stationNumber = gp->getStationNumber(packetParser->getStationId());
	// cerr << "samp station number=" << stationNumber << endl;

	int xmitIntervalUsec = gp->getXmitInterval(stationNumber) *
		USECS_PER_SEC;
	int xmitOffsetUsec = gp->getXmitOffset(stationNumber) * USECS_PER_SEC;

	// send a sample of GOES info
	const SampleTag* tag = gp->getGOESSampleTag(stationNumber);

	// Time of transmit interval.
	dsm_time_t txmit = tpack - (tpack % xmitIntervalUsec);

	// expected station transmission time
	dsm_time_t texpect = txmit + xmitOffsetUsec;

	int tdiff = (tpack - texpect) / USECS_PER_SEC;

	SampleT<float>* samp = getSample<float>(tag->getVariables().size());
	assert(samp->getDataLength() == 5);

	// sample time is middle of transmit interval
	samp->setTimeTag(txmit - xmitIntervalUsec / 2);
	samp->setId(tag->getId());

	float* fptr = samp->getDataPtr();
	fptr[0] = tdiff;
	fptr[1] = pinfo->getSignalStrength();
	fptr[2] = pinfo->getFreqOffset();
	fptr[3] = pinfo->getChannel();
	fptr[4] = pinfo->getStatusInt();
	_source.distribute(samp);

#define DEBUG
        DLOG(("packetParsrer->getSampleId()=") <<
            packetParser->getSampleId());
	if (packetParser->getSampleId() >= 0) {

            DLOG(("packetParsrer->getConfigId()=") <<
                packetParser->getConfigId());

	    tag = findSampleTag(packetParser->getConfigId(),
		packetParser->getStationId(),packetParser->getSampleId());

	    if (!tag) return;

	    size_t nvars = tag->getVariables().size();
	    // cerr << "nvars=" << nvars << endl;

	    samp = getSample<float>(nvars);

	    // cerr << "xmitInterval=" << xmitInterval << " tdiff=" << tdiff << endl;

	    samp->setTimeTag(txmit - xmitIntervalUsec / 2);
	    samp->setId(tag->getId());

	    packetParser->parseData(samp->getDataPtr(),nvars);

	    _source.distribute(samp);
	}
    }
    catch(const n_u::InvalidParameterException& e) {
	throw n_u::IOException(getName(),"readSamples",e.what());
    }
}

const GOESProject*
	PacketInputStream::getGOESProject(int configId) const
	throw(n_u::InvalidParameterException)
{
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
	GOESProject* gp = new GOESProject(project);
	projectsByConfigId[cid] = gp;
    }

    map<int,GOESProject*>::const_iterator pi =
    	projectsByConfigId.find(configId);

    if (pi == projectsByConfigId.end()) pi = projectsByConfigId.begin();
    
    return pi->second;
}

const SampleTag* PacketInputStream::findSampleTag(int configId,
	int goesId,int sampleId) throw(n_u::InvalidParameterException)
{
    const GOESProject* gp = getGOESProject(configId);
    int stationNumber = gp->getStationNumber(goesId);
    DLOG(("configId=") << configId << " stationNumber=" << stationNumber <<
        " sampleId=" << sampleId);
    const SampleTag* tag = gp->getSampleTag(stationNumber,sampleId);
    return tag;
}

