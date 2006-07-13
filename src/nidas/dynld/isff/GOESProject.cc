/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-23 12:30:55 -0600 (Tue, 23 May 2006) $

    $LastChangedRevision: 3364 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/dynld/SampleInputStream.cc $
 ********************************************************************
*/

#include <nidas/dynld/isff/GOESProject.h>
#include <nidas/dynld/isff/GOESOutput.h>

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

GOESProject::GOESProject(Project* p)
	throw(n_u::InvalidParameterException):
	project(p)

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

    ids = project->getParameter("goes_xmitOffsets");

    if (!ids)
	throw n_u::InvalidParameterException(
	    project->getName(),"goes_xmitOffsets","not found");

    if (ids->getType() != Parameter::INT_PARAM)
	throw n_u::InvalidParameterException(
	    project->getName(),"goes_xmitOffsets","not an integer");

    iids = static_cast<const ParameterT<int>*>(ids);
    xmitOffsets.clear();
    for (int i = 0; i < iids->getLength(); i++)
	xmitOffsets.push_back(iids->getValue(i));

    const char* goesVars[][3] = {
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

    SiteIterator si = project->getSiteIterator();

    for ( ; si.hasNext(); ) {
        Site* site = si.next();

	ProcessorIterator pi = site->getProcessorIterator();
	int stationNumber = site->getNumber();	// numbered from 0

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
		    dsm_sample_id_t maxSampleId = 0;
		    int xmitInterval = goesOutput->getXmitInterval();

		    for (int i = (signed) xmitIntervals.size();
				i <= stationNumber; i++)
					xmitIntervals.push_back(-1);
		    xmitIntervals[stationNumber] = xmitInterval;

		    for (; ti != tags.end(); ++ti) {
			SampleTag* tag = *ti;
			tag->setStation(stationNumber);
			tag->setDSMId(stationNumber+1);
			sampleTags.insert(tag);
			sampleTagsById[tag->getId()] = tag;
			maxSampleId = std::max(maxSampleId,tag->getShortId());
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

		    for (int i = (signed) goesTags.size();
			    i <= stationNumber; i++) goesTags.push_back(0);
		    if (!goesTags[stationNumber]) {
			SampleTag* gtag = new SampleTag();
			for (unsigned int i = 0; i <
			    sizeof(goesVars)/sizeof(goesVars[0]); i++) {
			    Variable* var = new Variable();
			    var->setName(goesVars[i][0]);
			    var->setUnits(goesVars[i][1]);
			    var->setLongName(goesVars[i][2]);
			    gtag->addVariable(var);
			}
			gtag->setStation(stationNumber);
			gtag->setSampleId(maxSampleId+1);
			gtag->setDSMId(stationNumber+1);
			gtag->setPeriod(xmitInterval);
			goesTags[stationNumber] = gtag;
			sampleTags.insert(gtag);
		    }
		}
	    }
	}
    }
}

GOESProject::~GOESProject()
{
    for (unsigned int i = 0; i < goesTags.size(); i++)
    	delete goesTags[i];
}

int GOESProject::getStationNumber(unsigned long goesId) const
	throw(n_u::InvalidParameterException)
{
    map<unsigned long,int>::const_iterator si = stationNumbersById.find(goesId);
    si = stationNumbersById.find(goesId);
    if (si != stationNumbersById.end()) return si->second;

    ostringstream ost;
    ost << "id: " << hex << goesId << " not found";
    throw n_u::InvalidParameterException(project->getName(),"goes_id", ost.str());
}

unsigned long GOESProject::getGOESId(int stationNum) const
	throw(n_u::InvalidParameterException)
{
    if (stationNum < 0 || stationNum >= (signed)goesIds.size()) return 0;
    return goesIds[stationNum];
}

int GOESProject::getXmitOffset(int stationNumber) const
	throw(n_u::InvalidParameterException)
{
    if (stationNumber < 0 || stationNumber >= (signed) xmitOffsets.size()) {
        ostringstream ost;
	ost << "not found for station " << stationNumber;
	throw n_u::InvalidParameterException(project->getName(),"goes_xmitOffset",
		ost.str());
    }
    return xmitOffsets[stationNumber];
}

int GOESProject::getXmitInterval(int stationNumber) const
	throw(n_u::InvalidParameterException)
{
    if (stationNumber < 0 || stationNumber >= (signed) xmitIntervals.size()) {
        ostringstream ost;
	ost << "not found for station " << stationNumber;
	throw n_u::InvalidParameterException(project->getName(),"goes transmit interval",
		ost.str());
    }
    return xmitIntervals[stationNumber];
}

const SampleTag* GOESProject::getSampleTag(
	int stationNumber, int sampleId) const
{
    // If sampleId is not parseable from the packet, it will
    // be -1.
    if (sampleId < 0) return 0;

    dsm_sample_id_t fullSampleId = sampleId;
    fullSampleId = SET_DSM_ID(fullSampleId,stationNumber+1);

    map<dsm_sample_id_t,const SampleTag*>::const_iterator ti =
    	sampleTagsById.find(fullSampleId);

    if (ti != sampleTagsById.end()) return ti->second;
    return 0;
}

const SampleTag* GOESProject::getGOESSampleTag(
	int stationNumber) const
	throw(n_u::InvalidParameterException)
{
    if (stationNumber < 0 || stationNumber >= (signed) goesTags.size())
        return 0;
    return goesTags[stationNumber];
}

const set<const SampleTag*>& GOESProject::getSampleTags()
	const
{
    return sampleTags;
}

