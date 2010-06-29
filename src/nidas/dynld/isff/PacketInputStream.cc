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
#include <nidas/dynld/isff/GOESProject.h>
#include <nidas/core/Project.h>
#include <nidas/core/SampleTag.h>
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
    _iochan(iochannel),_iostream(0),_packetParser(0)
{
    if (_iochan)
        _iostream = new IOStream(*_iochan,_iochan->getBufferSize());
}

/*
 * Copy constructor, with a new IOChannel.
 */
PacketInputStream::PacketInputStream(const PacketInputStream& x,
	IOChannel* iochannel):
    _iochan(iochannel),_iostream(0),_packetParser(0)
{
    if (_iochan)
        _iostream = new IOStream(*_iochan,_iochan->getBufferSize());
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
    delete _iostream;
    delete _iochan;
    delete _packetParser;
    map<int,GOESProject*>::const_iterator pi =
    	_projectsByConfigId.begin();
    for ( ; pi != _projectsByConfigId.end(); ++pi) delete pi->second;
}

string PacketInputStream::getName() const
{
    if (_iochan) return string("PacketInputStream: ") + _iochan->getName();
    return string("PacketInputStream");
}

list<const SampleTag*> PacketInputStream::getSampleTags() const
{
    const GOESProject* gp = getGOESProject(0);
    return gp->getSampleTags();
}

void PacketInputStream::init() throw()
{
    if (!_iostream)
	_iostream = new IOStream(*_iochan,_iochan->getBufferSize());

    // throws ParseException if internal regular expressions don't
    // compile - programmer error.
    try {
	if (!_packetParser) _packetParser = new PacketParser();
    }
    catch (const n_u::ParseException& e) {
	n_u::Logger::getInstance()->log(LOG_WARNING,
		"Programmer error: %s",e.what());
	throw e;
    }
}

void PacketInputStream::close() throw(n_u::IOException)
{
    delete _iostream;
    _iostream = 0;
    delete _packetParser;
    _packetParser = 0;
    _iochan->close();
}

void PacketInputStream::readSamples() throw(n_u::IOException)
{
    char packet[1024];
    size_t len = _iostream->readUntil(packet,sizeof(packet),'\n');

    if (packet[len-1] != '\n')
    	throw n_u::IOException(getName(),"readUntil",
		"no termination character found");

    // toss empty packets
    size_t i;
    for (i = 0; i < len && ::isspace(packet[i]); i++);
    if (i == len) return;

    PacketParser::packet_type ptype;

    try {
	ptype = _packetParser->parse(packet);
    }
    catch (const n_u::ParseException& e) {
	n_u::Logger::getInstance()->log(LOG_WARNING,
	    "%s: %s",getName().c_str(),e.what());
	return;
    }

#ifdef DEBUG
    cerr << hex << _packetParser->getStationId() << dec << ' ' << 
    	_packetParser->getPacketTime().format(true,"%c") << ' ' <<
	*_packetParser->getPacketInfo() << endl;
#endif

    switch(ptype) {
    case PacketParser::NESDIS_PT:
	break;
    default:
        return;
    }

    dsm_time_t tpack = _packetParser->getPacketTime().toUsecs();
    const PacketInfo* pinfo = _packetParser->getPacketInfo();

    try {

	const GOESProject* gp = getGOESProject(_packetParser->getConfigId());
	int stationNumber = gp->getStationNumber(_packetParser->getStationId());

	int xmitIntervalUsec = gp->getXmitInterval(stationNumber) *
		USECS_PER_SEC;
	int xmitOffsetUsec = gp->getXmitOffset(stationNumber) * USECS_PER_SEC;

	// send a sample of GOES info
	const SampleTag* tag = gp->getGOESSampleTag(stationNumber);
        if (!tag) return;

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

// #define DEBUG
        DLOG(("packetParser->getSampleId()=") <<
            _packetParser->getSampleId());
	if (_packetParser->getSampleId() >= 0) {

            DLOG(("packetParser->getConfigId()=") <<
                _packetParser->getConfigId());

	    tag = findSampleTag(_packetParser->getConfigId(),
		_packetParser->getStationId(),_packetParser->getSampleId());

	    if (!tag) return;

	    size_t nvars = tag->getVariables().size();
	    // cerr << "nvars=" << nvars << endl;

	    samp = getSample<float>(nvars);

	    // cerr << "xmitInterval=" << xmitInterval << " tdiff=" << tdiff << endl;

	    samp->setTimeTag(txmit - xmitIntervalUsec / 2);
	    samp->setId(tag->getId());

	    _packetParser->parseData(samp->getDataPtr(),nvars);

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
    if (_projectsByConfigId.size() == 0) {
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
	_projectsByConfigId[cid] = gp;
    }

    map<int,GOESProject*>::const_iterator pi =
    	_projectsByConfigId.find(configId);

    if (pi == _projectsByConfigId.end()) pi = _projectsByConfigId.begin();
    
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

