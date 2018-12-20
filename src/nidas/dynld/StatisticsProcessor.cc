// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "StatisticsProcessor.h"
#include <nidas/core/SampleOutputRequestThread.h>
#include <nidas/core/Project.h>
#include <nidas/core/Variable.h>
#include <nidas/core/Site.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(StatisticsProcessor);

StatisticsProcessor::StatisticsProcessor():
    SampleIOProcessor(false),
    _cruncherListMutex(),_connectedSources(),_connectedOutputs(),
    _crunchers(),_infoBySampleId(),
    _startTime(LONG_LONG_MIN),_endTime(LONG_LONG_MAX),_statsPeriod(0.0),
    _fillGaps(false),_cntsNames()
{
    setName("StatisticsProcessor");
}

StatisticsProcessor::~StatisticsProcessor()
{
    std::set<SampleOutput*>::const_iterator oi = _connectedOutputs.begin();
    for ( ; oi != _connectedOutputs.end(); ++oi) {
        SampleOutput* output = *oi;

        _cruncherListMutex.lock();
        list<StatisticsCruncher*>::const_iterator ci;
        for (ci = _crunchers.begin(); ci != _crunchers.end(); ++ci) {
            StatisticsCruncher* cruncher = *ci;
            cruncher->flush();
            cruncher->removeSampleClient(output);
        }
        _cruncherListMutex.unlock();

        output->flush();
        try {
            output->close();
        }
        catch (const n_u::IOException& ioe) {
            n_u::Logger::getInstance()->log(LOG_ERR,
                "%s: error closing %s: %s",
                getName().c_str(),output->getName().c_str(),ioe.what());
        }

        SampleOutput* orig = output->getOriginal();
        if (orig != output) delete output;
    }
    list<StatisticsCruncher*>::const_iterator ci;
    for (ci = _crunchers.begin(); ci != _crunchers.end(); ++ci) {
        StatisticsCruncher* cruncher = *ci;
        delete cruncher;
    }
}

void StatisticsProcessor::flush() throw()
{
    std::set<SampleOutput*>::const_iterator oi = _connectedOutputs.begin();
    for ( ; oi != _connectedOutputs.end(); ++oi) {
        SampleOutput* output = *oi;

        _cruncherListMutex.lock();
        list<StatisticsCruncher*>::const_iterator ci;
        for (ci = _crunchers.begin(); ci != _crunchers.end(); ++ci) {
            StatisticsCruncher* cruncher = *ci;
            cruncher->flush();
        }
        _cruncherListMutex.unlock();

        output->flush();
    }
}

void StatisticsProcessor::addRequestedSampleTag(SampleTag* tag)
{

    // At this point this SampleTag doesn't contain any
    // <variable> tags, but does contain a parameter
    // called "invars" containing the variable names
    // that are to be processed.
    //
    // The number of actual output SampleTags from this processor
    // may be greater than the number of SampleTags added
    // here. For example: if a SampleTag is added here
    // with a variable called "P", then output SampleTags
    // will be created for every "P" from every station.

    if (!tag->isEnabled()) return;

    const std::list<const Parameter*>& parms = tag->getParameters();
    std::list<const Parameter*>::const_iterator pi;

    const Parameter* vparm = 0;

    struct OutputInfo outputInfo;
    outputInfo.countsName = "";
    outputInfo.type = StatisticsCruncher::STATS_UNKNOWN;
    outputInfo.higherMoments = false;

    // If a wind direction statistics group for a "u" variable
    // (lower case indicating it is from a 3D sonic)
    // and horizontal rotations of 3D sonics are not enabled,
    // then skip that group.
    bool winddirCheck = false;

    for (pi = parms.begin(); pi != parms.end(); ++pi) {
        const Parameter* p = *pi;
	if (p->getType() == Parameter::STRING_PARAM &&
		p->getName() == "invars" && p->getLength() > 0) {
	    vparm = p;
	}
	else if (p->getType() == Parameter::STRING_PARAM &&
		p->getName() == "type" && p->getLength() == 1) {
	    outputInfo.type =
	    	StatisticsCruncher::getStatisticsType(p->getStringValue(0));
            // Only compute WINDDIR statistics if horizontal rotations
            // are enabled.
            if (outputInfo.type == StatisticsCruncher::STATS_WINDDIR) {
                const Parameter* parameter =
                    Project::getInstance()->getParameter("wind3d_horiz_rotation");
                if (parameter &&
                    (parameter->getType() == Parameter::BOOL_PARAM ||
                    parameter->getType() == Parameter::INT_PARAM) &&
                    parameter->getLength() == 1) {
                    bool val = (bool) parameter->getNumericValue(0);
                    if (!val) winddirCheck = true;
                }
            }
	}
	else if (p->getType() == Parameter::STRING_PARAM &&
		p->getName() == "counts" && p->getLength() == 1) {
	    outputInfo.countsName = Project::getInstance()->expandString(p->getStringValue(0));
	    
	}
	else if (p->getType() == Parameter::BOOL_PARAM &&
		p->getName() == "highmoments" && p->getLength() == 1) {
	    outputInfo.higherMoments = p->getNumericValue(0) != 0;
	}
        else if (p->getType() == Parameter::INT_PARAM &&
		p->getName() == "station" && p->getLength() == 1) {
	}
	else throw n_u::InvalidParameterException(getName(),
		"unknown statistics parameter",p->getName());
    }
    if (!vparm) {
	ostringstream ost;
	dsm_sample_id_t id = tag->getId();
	ost << "sample id=" << GET_DSM_ID(id) << ',' << GET_SPS_ID(id);
        throw n_u::InvalidParameterException(
	    getName(),ost.str(),"has no \"invars\" parameter");
    }
    if (outputInfo.type == StatisticsCruncher::STATS_UNKNOWN) {
	ostringstream ost;
	dsm_sample_id_t id = tag->getId();
	ost << "sample id=" << GET_DSM_ID(id) << ',' << GET_SPS_ID(id);
        throw n_u::InvalidParameterException(
	    getName(),ost.str(),"has no \"type\" parameter");
    }

    if (tag->getRate() <= 0.0) {
	ostringstream ost;
	dsm_sample_id_t id = tag->getId();
	ost << "sample id=" << GET_DSM_ID(id) << ',' << GET_SPS_ID(id);
        throw n_u::InvalidParameterException(
	    getName(),ost.str(),"has an unknown period or rate");
    }

    float sPeriod = tag->getPeriod();
    if (_statsPeriod > 0.0) {
        if (fabs(sPeriod - _statsPeriod) > 1.e-3) {
            ostringstream ost;
            dsm_sample_id_t id = tag->getId();
            ost << "average period (" << tag->getPeriod() <<
                " secs) for sample id=" << GET_DSM_ID(id) << ',' << GET_SPS_ID(id) <<
                " differs from period of previous sample (" <<
                _statsPeriod << " secs)";
            throw n_u::InvalidParameterException(getName(),"rate",
                ost.str());
        }
    } 
    else _statsPeriod = sPeriod;

    vector<string> vnames;
    for (int i = 0; i < vparm->getLength(); i++) {
	Variable* var = new Variable();
	string vname = Project::getInstance()->expandString(vparm->getStringValue(i));
        if (winddirCheck && vname.length() > 1 && vname[0] == 'u') {
            ILOG(("wind direction statistics group for %s skipped since horizontal rotations are not enabled",vname.c_str()));
            return;
        }
	var->setName(vname);
	tag->addVariable(var);
    }

    // initial sample id of the requested tag.
    tag->setSampleId(getRequestedSampleTags().size() + 1);

    // save stuff that doesn't fit in the sample tag.
    // cerr << "tag id=" << tag->getDSMId() << ',' << tag->getSpSId() << " statstype=" << outputInfo.type << endl;
    _infoBySampleId[tag->getId()] = outputInfo;

    SampleIOProcessor::addRequestedSampleTag(tag);
}

string StatisticsProcessor::getUniqueCountsName(const string& val) 
{
    if (_cntsNames.find(val) == _cntsNames.end()) {
        _cntsNames.insert(val);
        return val;
    }
    for (int i = 1; ; i++) {
        ostringstream ost;
        ost << val << '_' << i;
        if (_cntsNames.find(ost.str()) == _cntsNames.end()) {
            _cntsNames.insert(ost.str());
            return ost.str();
        }
    }
    return "";
}

void StatisticsProcessor::selectRequestedSampleTags(const vector<unsigned int>& sampleIds)
{
    _tagsMutex.lock();
    std::list<SampleTag*> tags = _requestedTags;
    _tagsMutex.unlock();

    list<SampleTag*>::iterator ti = tags.begin();
    for ( ; ti != tags.end(); ) {
        SampleTag* tag = *ti;
        unsigned int id = tag->getSpSId() - getSampleId();
        if (std::find(sampleIds.begin(),sampleIds.end(),id) != sampleIds.end()) {
            ti = tags.erase(ti);
        }
        else ++ti;
    }

    ti = tags.begin();
    for ( ; ti != tags.end(); ++ti) {
        SampleTag* tag = *ti;
        removeRequestedSampleTag(tag);
    }
}

void StatisticsProcessor::connect(SampleSource* source) throw()
{
// #define DEBUG
#ifdef DEBUG
    cerr << "StatisticsProcessor connect, #of tags=" <<
    	source->getSampleTags().size() << endl;
#endif

    source = source->getProcessedSampleSource();
    assert(source);

    // In order to improve support for the ISFS Wisard motes, where
    // the same variable can appear in more than one sample 
    // (for example if a sensor's input is moved between motes), this code
    // allows matching of a variable from more than one input sample.
    map<dsm_sample_id_t, StatisticsCruncher*> crunchersByOutputId;

    // loop over requested sample tags
    list<const SampleTag*> reqtags = getRequestedSampleTags();
    list<const SampleTag*>::const_iterator reqti = reqtags.begin();
    for ( ; reqti != reqtags.end(); ++reqti ) {
	const SampleTag* reqtag = *reqti;

        // make sure we have at least one variable
	if (reqtag->getVariables().size() == 0) continue;

        // first requested variable in the sample
	const Variable* reqvar = reqtag->getVariables().front();

	// find all matches against first requested variable
        // of each requested statistics sample
        list<const SampleTag*> ptags = source->getSampleTags();
        list<const SampleTag*>::const_iterator inti =  ptags.begin();

        // The first requested variable may match multiple
        // samples, as it is a kind of wild card:  w.2m may
        // match a variable at multiple sites. So we don't
        // break out when we find one match.
        int nmatch = 0;

	for ( ; inti != ptags.end(); ++inti) {
	    const SampleTag* intag = *inti;
	    for (VariableIterator invi = intag->getVariableIterator();
	    	invi.hasNext(); ) {
		const Variable* invar = invi.next();

#ifdef DEBUG
                cerr << "invar=" << invar->getName() << '(' <<
                    (invar->getSampleTag() ? invar->getSampleTag()->getDSMId() : 0) <<
                    ',' <<
                    (invar->getSampleTag() ? invar->getSampleTag()->getSpSId() : 0) <<
                    ") site=" << (invar->getSite() ? invar->getSite()->getSuffix() : "unk") << 
                    ") station=" << invar->getStation() << 
                    ", reqvar=" << reqvar->getName() << '(' <<
                    (reqvar->getSampleTag() ? reqvar->getSampleTag()->getDSMId() : 0) <<
                    ',' <<
                    (reqvar->getSampleTag() ? reqvar->getSampleTag()->getSpSId() : 0) <<
                    ") site=" << (reqvar->getSite() ? reqvar->getSite()->getSuffix() : "unk") << 
                    ") station=" << reqvar->getStation() << 
                    ", match=" << (*invar == *reqvar) <<
                    ", closeMatch=" << invar->closeMatch(*reqvar) << endl;
#endif
		
		// variable match with first requested variable
                if (invar->closeMatch(*reqvar)) {

                    assert(invar->getSite());

#ifdef DEBUG
                    if (reqvar->getName().substr(0,4) == "u.2m") {
                        cerr << "StatisticsProcessor::connect: match, reqvar=" << reqvar->getName() <<
                        " invar=" << invar->getName() << ", site=" << invar->getSite()->getName() << '(' << invar->getStation() << ')' << endl;
                    }
#endif
		    struct OutputInfo info = _infoBySampleId[reqtag->getId()];
                    // cerr << "reqtag id=" << reqtag->getDSMId() << ',' << reqtag->getSpSId() << " statstype=" << info.type << endl;
                    /* reqtag is a SampleTag, containing variables which possibly
                     * just have short names, without a site suffix.
                     * The SampleTag also has a dsm id of 0. The sample id of the tag
                     * is just incremented by one as they are read from the XML.
                     */
                    SampleTag newtag(*reqtag);

                    // Set the site of the requested variables to the
                    // site of the first matched variable
                    // All data for this statistics group comes from that site
                    for (unsigned int i = 0; i < newtag.getVariables().size(); i++) {
                        newtag.getVariable(i).setSite(invar->getSite());
                    }
                    newtag.getVariable(0).setStation(invar->getStation());

                    // make this tag unique within this processor
                    newtag.setDSMId(intag->getDSMId());

#ifdef DEBUG
                    if (reqvar->getName().substr(0,5) == "Vbatt") {
                        cerr << "StatisticsProcessor::connect: vars=";
                        for (unsigned int i = 0; i < newtag.getVariables().size(); i++)
                            cerr << newtag.getVariable(i).getName() << ':' <<
                                newtag.getVariable(i).getSite()->getName() << '(' <<
                                newtag.getVariable(i).getStation() << "), ";
                        cerr << endl;
                    }
#endif

                    // Create a StatisticsCruncher if it doesn't yet exist
                    // for this requested sample.
		    StatisticsCruncher* cruncher = crunchersByOutputId[newtag.getId()];
                    if (!cruncher) {
                        cruncher = new StatisticsCruncher(this,
                                &newtag,info.type, info.countsName,
                                info.higherMoments);
                        cruncher->setStartTime(getStartTime());
                        cruncher->setEndTime(getEndTime());
                        cruncher->setFillGaps(getFillGaps());

                        _cruncherListMutex.lock();
                        _crunchers.push_back(cruncher);
                        _cruncherListMutex.unlock();
                        crunchersByOutputId[newtag.getId()] = cruncher;
                        cruncher->connect(source);

                        list<const SampleTag*> tags = cruncher->getSampleTags();
                        list<const SampleTag*>::const_iterator ti = tags.begin();
                        for ( ; ti != tags.end(); ++ti) {
                            const SampleTag* tag = *ti;
#ifdef DEBUG
                            cerr << "adding sample tag, id=" << tag->getDSMId() << ',' << tag->getSpSId() << ", nvars=" << tag->getVariables().size() << " var[0]=" <<
                                tag->getVariables()[0]->getName() << endl;
#endif
                            addSampleTag(tag);
                        }
                    }
                    nmatch++;
                    break;
		}
#ifdef DEBUG
                else
                    cerr << "no match, invar=" << invar->getName() <<
                        " reqvar=" << reqvar->getName() << endl;
                        
#endif
	    }
	}
	if (nmatch == 0)
	    n_u::Logger::getInstance()->log(LOG_WARNING,
		"%s: no match for variable %s",
		getName().c_str(),reqvar->getName().c_str());
    }

    _cruncherListMutex.lock();

    // on first SampleSource connection, request output connections
    //
    // Currently this code will not work correctly if we get a connection
    // from more than one SampleSource.  The NetcdfRPCOutput needs
    // to know all the expected SampleTags that it will be sending
    // before a connection request is made.
    // This is not an issue currently since there will be only
    // one connection from a SamplePipeline.

    // When this is doing post-processing from statsproc it may be
    // best to make a synchronous connection request. When doing
    // 5 minute statistics though the connection should be finished
    // before the first output sample is ready.
    //
    // When CHATS high rate processing to NetCDF was running, in addition to
    // statsproc for BEACHON_SRM, saw some 1 hour data gaps in BEACHON_SRM netcdf
    // data at the beginning of the day, so apparently the connection to
    // nc_server was taking some time.

    // On Jul 21 made a fix to statsproc to issue synchronous connect requests
    // when not reading from a socket.

    if (_connectedSources.size() == 0) {
        const list<SampleOutput*>& outputs = getOutputs();
        list<SampleOutput*>::const_iterator oi = outputs.begin();
        for ( ; oi != outputs.end(); ++oi) {
            SampleOutput* output = *oi;
            output->addSourceSampleTags(getSampleTags());
            SampleOutputRequestThread::getInstance()->addConnectRequest(output,this,0);
        }
    }

    _connectedSources.insert(source);
    _cruncherListMutex.unlock();
}

void StatisticsProcessor::disconnect(SampleSource* source) throw()
{
    source = source->getProcessedSampleSource();
    if (!source) return;

    _cruncherListMutex.lock();
    list<StatisticsCruncher*>::const_iterator ci;
    for (ci = _crunchers.begin(); ci != _crunchers.end(); ++ci) {
        StatisticsCruncher* cruncher = *ci;
	cruncher->disconnect(source);
	cruncher->flush();
    }
    _connectedSources.erase(source);
    _cruncherListMutex.unlock();
}
 
void StatisticsProcessor::connect(SampleOutput* output) throw()
{

    _cruncherListMutex.lock();
#ifdef DEBUG
    cerr << "StatisticsProcessor::connect, output=" << output->getName() << endl;
#endif
    list<StatisticsCruncher*>::const_iterator ci;
    for (ci = _crunchers.begin(); ci != _crunchers.end(); ++ci) {
        StatisticsCruncher* cruncher = *ci;
	cruncher->addSampleClient(output);
    }
    _connectedOutputs.insert(output);
    _cruncherListMutex.unlock();
}
 
void StatisticsProcessor::disconnect(SampleOutput* output) throw()
{

    _cruncherListMutex.lock();
    list<StatisticsCruncher*>::const_iterator ci;
    for (ci = _crunchers.begin(); ci != _crunchers.end(); ++ci) {
        StatisticsCruncher* cruncher = *ci;
        // It is tempting to do a cruncher->flush() here, but
        // one shouldn't because there may be more than
        // one client of the cruncher.
        //
        // Also be wary of calls, like flush(), which may cause
        // this method to be called again, if the SampleOutput
        // is in a disconnected or error state. That will cause
        // a deadlock on _cruncherListMutex here.
	cruncher->removeSampleClient(output);
    }
    _cruncherListMutex.unlock();
    _connectedOutputs.erase(output);

    output->flush();
    try {
        output->close();
    }
    catch (const n_u::IOException& ioe) {
        n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: error closing %s: %s",
            getName().c_str(),output->getName().c_str(),ioe.what());
    }

    SampleOutput* orig = output->getOriginal();
    if (orig != output)
        SampleOutputRequestThread::getInstance()->addDeleteRequest(output);

    // reschedule a request for the original output.
    int delay = orig->getReconnectDelaySecs();
    if (delay < 0) return;
    SampleOutputRequestThread::getInstance()->addConnectRequest(orig,this,delay);
}

