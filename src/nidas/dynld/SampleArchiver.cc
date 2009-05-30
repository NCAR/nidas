/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/dynld/SampleArchiver.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMServer.h>
#include <nidas/core/SampleInput.h>

#include <nidas/util/Logger.h>

#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(SampleArchiver)

SampleArchiver::SampleArchiver(): SampleIOProcessor(),_input(0),
    _nsampsLast(0),_nbytesLast(0)
{
    setName("SampleArchiver");
}

SampleArchiver::SampleArchiver(const SampleArchiver& x):
    SampleIOProcessor((const SampleIOProcessor&)x),_input(0),
    _nsampsLast(0),_nbytesLast(0)
{
    setName("SampleArchiver");
}

SampleArchiver::~SampleArchiver()
{
}

SampleArchiver* SampleArchiver::clone() const {
    return new SampleArchiver(*this);
}

void SampleArchiver::connect(SampleInput* newinput) throw(n_u::IOException)
{
    _statusMutex.lock();
    _input = newinput;
    _statusMutex.unlock();
    SampleTagIterator ti = _input->getSampleTagIterator();
    for ( ; ti.hasNext(); ) {
	const SampleTag* stag = ti.next();
	addSampleTag(new SampleTag(*stag));
    }
    SampleIOProcessor::connect(_input);
}
 
void SampleArchiver::disconnect(SampleInput* oldinput) throw()
{
    if (!_input) return;
    assert(_input == oldinput);

    const set<SampleOutput*>& cnctdOutputs = getConnectedOutputs();
    set<SampleOutput*>::const_iterator oi =
    	cnctdOutputs.begin();
    for ( ; oi != cnctdOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
        _input->removeSampleClient(output);
    }

    SampleIOProcessor::disconnect(_input);
    _statusMutex.lock();
    _input = 0;
    _statusMutex.unlock();
}
 
void SampleArchiver::connected(SampleOutput* orig,SampleOutput* output) throw()
{
    assert(_input);
    SampleIOProcessor::connected(orig,output);
    _input->addSampleClient(output);
    nidas::dynld::FileSet* fset = dynamic_cast<nidas::dynld::FileSet*>(output->getIOChannel());
    if (fset) {
        _statusMutex.lock();
        _filesets.push_back(fset);
        _statusMutex.unlock();
    }
}
 
void SampleArchiver::disconnected(SampleOutput* output) throw()
{
    if (_input) _input->removeSampleClient(output);
    SampleIOProcessor::disconnected(output);
    nidas::dynld::FileSet* fset = dynamic_cast<nidas::dynld::FileSet*>(output->getIOChannel());
    if (fset) {
        _statusMutex.lock();
        list<const FileSet*>::iterator fi =
            std::find(_filesets.begin(),_filesets.end(),fset);
        if (fi != _filesets.end()) _filesets.erase(fi);
        _statusMutex.unlock();
    }
}

void SampleArchiver::printStatus(ostream& ostr,float deltat,int &zebra)
    throw()
{
    const char* oe[2] = {"odd","even"};

    n_u::Autolock statusMutex(_statusMutex);

    ostr <<
        "<tr class=" << oe[zebra++%2] << "><td align=left>archive</td>";
    dsm_time_t tt = 0LL;
    if (_input) tt = _input->getLastDistributedTimeTag();
    if (tt > 0LL)
        ostr << "<td>" << n_u::UTime(tt).format(true,"%Y-%m-%d&nbsp;%H:%M:%S.%1f") << "</td>";
    else
        ostr << "<td><font color=red>Not active</font></td>";
    size_t nsamps = (_input ? _input->getNumDistributedSamples() : 0);
    float samplesps = (float)(nsamps - _nsampsLast) / deltat;

    long long nbytes = (_input ? _input->getNumDistributedBytes() : 0);
    float bytesps = (float)(nbytes - _nbytesLast) / deltat;

    _nsampsLast = nsamps;
    _nbytesLast = nbytes;

    bool warn = fabs(bytesps) < 0.0001;
    ostr <<
        (warn ? "<td><font color=red><b>" : "<td>") <<
        fixed << setprecision(1) << samplesps <<
        (warn ? "</b></font></td>" : "</td>") <<
        (warn ? "<td><font color=red><b>" : "<td>") <<
        setprecision(0) << bytesps <<
        (warn ? "</b></font></td>" : "</td>");
    ostr << "<td></td><td></td></tr>\n";

    list<const nidas::dynld::FileSet*>::const_iterator fi = _filesets.begin();
    for ( ; fi != _filesets.end(); ++fi) {
        const nidas::dynld::FileSet* fset = *fi;
        if (fset) {
            ostr <<
                "<tr class=" << oe[zebra++%2] << "><td align=left colspan=3>" <<
                fset->getCurrentName() << "</td>";

            long long nbytes = fset->getFileSize();
            float bytesps = (float)(nbytes - _nbytesLastByFileSet[fset]) / deltat;

            _nbytesLastByFileSet[fset] = nbytes;

            bool warn = fabs(bytesps) < 0.0001;
            ostr <<
                (warn ? "<td><font color=red><b>" : "<td>") <<
                setprecision(0) << bytesps <<
                (warn ? "</b></font></td>" : "</td>") <<
                "<td>" << setprecision(2) << nbytes / 1000000.0 << "</td>";
            int err = fset->getLastErrno();
            warn = err != 0;
            ostr <<
                (warn ? "<td align=left>font color=red><b>" : "<td align=left>") <<
                "status=" <<
                (warn ? strerror(err) : "OK") <<
                (warn ? "</b></font></td>" : "</td>");
            ostr << "</tr>\n";
        }
    }
}
