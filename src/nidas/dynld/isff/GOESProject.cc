/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/dynld/isff/GOESProject.h>
#include <nidas/dynld/isff/GOESOutput.h>
#include <nidas/core/SampleIOProcessor.h>
#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/Variable.h>

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

GOESProject::GOESProject(Project* p)
	throw(n_u::InvalidParameterException):
	_project(p)

{
    const Parameter* ids = _project->getParameter("goes_ids");

    if (!ids)
	throw n_u::InvalidParameterException(
	    _project->getName(),"goes_ids","not found");

    if (ids->getType() != Parameter::INT_PARAM)
	throw n_u::InvalidParameterException(
	    _project->getName(),"goes_ids","not an integer");

    const ParameterT<int>* iids = static_cast<const ParameterT<int>*>(ids);
    for (int i = 0; i < iids->getLength(); i++) {
	unsigned long goesId = (unsigned) iids->getValue(i);
	_goesIds.push_back(goesId);
	_stationNumbersById[goesId] = i;
    }

    ids = _project->getParameter("goes_xmitOffsets");

    if (!ids)
	throw n_u::InvalidParameterException(
	    _project->getName(),"goes_xmitOffsets","not found");

    if (ids->getType() != Parameter::INT_PARAM)
	throw n_u::InvalidParameterException(
	    _project->getName(),"goes_xmitOffsets","not an integer");

    iids = static_cast<const ParameterT<int>*>(ids);
    _xmitOffsets.clear();
    for (int i = 0; i < iids->getLength(); i++)
	_xmitOffsets.push_back(iids->getValue(i));

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

    SiteIterator si = _project->getSiteIterator();

    for ( ; si.hasNext(); ) {
        Site* site = si.next();

	ProcessorIterator pi = site->getProcessorIterator();
	// 0 is the "non" station. Otherwise stations are numbered from 1
	int stationNumber = site->getNumber();

	for ( ; pi.hasNext(); ) {
	    SampleIOProcessor* proc = pi.next();
	    const list<SampleOutput*>& outputs = proc->getOutputs();
	    list<SampleOutput*>::const_iterator oi = outputs.begin();
	    for ( ; oi != outputs.end(); ++oi) {
		SampleOutput* output = *oi;
		GOESOutput* goesOutput =
		    dynamic_cast<nidas::dynld::isff::GOESOutput*>(output);
		if (goesOutput) {
		    dsm_sample_id_t maxSampleId = 0;
		    int xmitInterval = goesOutput->getXmitInterval();

		    for (int i = (signed) _xmitIntervals.size();
				i <= stationNumber; i++)
					_xmitIntervals.push_back(-1);
		    _xmitIntervals[stationNumber] = xmitInterval;

		    list<const SampleTag*> tags =
			    goesOutput->getRequestedSampleTags();
		    list<const SampleTag*>::const_iterator ti = tags.begin();
		    for (; ti != tags.end(); ++ti) {
			const SampleTag* itag = *ti;
                        SampleTag* newtag = new SampleTag(*itag);
			newtag->setSiteAttributes(site);
			newtag->setDSMId(stationNumber);
			_sampleTags.push_back(newtag);
			_sampleTagsById[newtag->getId()] = newtag;
			maxSampleId = std::max(maxSampleId,newtag->getSpSId());
			// Copy units and long name attributes
			// from the sensor variables.
			for (unsigned int iv = 0;
				iv < newtag->getVariables().size(); iv++) {
			    Variable& v1 = newtag->getVariable(iv);
			    if (v1.getUnits().length() == 0) {
				VariableIterator vi2 =
					_project->getVariableIterator();
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

		    for (int i = (signed) _goesTags.size();
			    i <= stationNumber; i++) _goesTags.push_back(0);
		    if (!_goesTags[stationNumber]) {
			SampleTag* gtag = new SampleTag();
			for (unsigned int i = 0; i <
			    sizeof(goesVars)/sizeof(goesVars[0]); i++) {
			    Variable* var = new Variable();
			    var->setName(goesVars[i][0]);
			    var->setUnits(goesVars[i][1]);
			    var->setLongName(goesVars[i][2]);
			    gtag->addVariable(var);
			}
			gtag->setSiteAttributes(site);
			gtag->setSampleId(maxSampleId+1);
			gtag->setDSMId(stationNumber);
			gtag->setPeriod(xmitInterval);
			_goesTags[stationNumber] = gtag;
			_sampleTags.push_back(gtag);
		    }
		}
	    }
	}
    }
}

GOESProject::~GOESProject()
{
    list<SampleTag*>::const_iterator ti = _sampleTags.begin();
    for ( ; ti != _sampleTags.end(); ++ti)
    	delete *ti;
}

int GOESProject::getStationNumber(unsigned long goesId) const
	throw(n_u::InvalidParameterException)
{
    map<unsigned long,int>::const_iterator si = _stationNumbersById.find(goesId);
    si = _stationNumbersById.find(goesId);
    if (si != _stationNumbersById.end()) return si->second;

    ostringstream ost;
    ost << "id: " << hex << goesId << " not found";
    throw n_u::InvalidParameterException(_project->getName(),"goes_id", ost.str());
}

unsigned long GOESProject::getGOESId(int stationNum) const
	throw(n_u::InvalidParameterException)
{
    if (stationNum < 0 || stationNum >= (signed)_goesIds.size()) return 0;
    return _goesIds[stationNum];
}

int GOESProject::getXmitOffset(int stationNumber) const
	throw(n_u::InvalidParameterException)
{
    if (stationNumber < 0 || stationNumber >= (signed) _xmitOffsets.size()) {
        ostringstream ost;
	ost << "not found for station " << stationNumber;
	throw n_u::InvalidParameterException(_project->getName(),"goes_xmitOffset",
		ost.str());
    }
    return _xmitOffsets[stationNumber];
}

int GOESProject::getXmitInterval(int stationNumber) const
	throw(n_u::InvalidParameterException)
{
    if (stationNumber < 0 || stationNumber >= (signed) _xmitIntervals.size()) {
        ostringstream ost;
	ost << "not found for station " << stationNumber;
	throw n_u::InvalidParameterException(_project->getName(),"goes transmit interval",
		ost.str());
    }
    return _xmitIntervals[stationNumber];
}

const SampleTag* GOESProject::getSampleTag(
	int stationNumber, int sampleId) const
{
    // If sampleId is not parseable from the packet, it will
    // be -1.
    if (sampleId < 0) return 0;

    dsm_sample_id_t fullSampleId = sampleId;
    fullSampleId = SET_DSM_ID(fullSampleId,stationNumber);

    map<dsm_sample_id_t,const SampleTag*>::const_iterator ti =
    	_sampleTagsById.find(fullSampleId);

    if (ti != _sampleTagsById.end()) return ti->second;
    return 0;
}

const SampleTag* GOESProject::getGOESSampleTag(
	int stationNumber) const
	throw(n_u::InvalidParameterException)
{
    if (stationNumber < 0 || stationNumber >= (signed) _goesTags.size())
        return 0;
    return _goesTags[stationNumber];
}
