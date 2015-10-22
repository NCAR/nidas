// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 8; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/dynld/raf/SyncRecordReader.h>
#include <nidas/dynld/raf/SyncRecordSource.h>
#include <nidas/core/CalFile.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/Logger.h>

#include <limits>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

struct SyncReaderStop : public StopSignal
{
    SyncReaderStop(SyncRecordReader* target) :
        StopSignal(),
        _target(target)
    {}

    virtual void stop()
    {
        _target->endOfStream();
    }

    virtual
    ~SyncReaderStop()
    {}

    SyncRecordReader* _target;

private:
    SyncReaderStop(const SyncReaderStop&);
    SyncReaderStop& operator=(const SyncReaderStop&);
};

SyncRecordReader::SyncRecordReader(IOChannel*iochan):
    inputStream(new SampleInputStream(iochan)),
    syncServer(0),_read_sync_server(false),
    headException(0),
    sampleTags(),variables(),variableMap(),
    numDataValues(0),projectName(),aircraftName(),flightName(),
    softwareVersion(), startTime(0),_debug(false),
    _header(), _qcond(), _eoq(false),_syncRecords()
{
    init();
}


SyncRecordReader::SyncRecordReader(SyncServer* ss):
    inputStream(0),
    syncServer(0),_read_sync_server(false),
    headException(0),
    sampleTags(),variables(),variableMap(),
    numDataValues(0),projectName(),aircraftName(),flightName(),
    softwareVersion(), startTime(0),_debug(false),
    _header(), _qcond(), _eoq(false), _syncRecords()
{
    // We have two possible implementations using the SyncServer: let the
    // SyncServer run in its own thread to keep pushing samples down the
    // pipeline to us, or keep a reference to it and explicitly read more
    // samples into it when we need them.  The latter does not quite work,
    // so we use the former.  Setting _read_sync_server false selects the
    // former.

    syncServer = ss;

    // Setup this reader as a sample client of the SyncServer, replacing
    // the default output.
    ss->addSampleClient(this);
    ss->setStopSignal(new SyncReaderStop(this));
    
    // Rather than start the SyncServer so it triggers the header from
    // SyncRecordSource, request the header directly so it can be read
    // immediately.
    ss->init();
    ss->sendHeader();

    init();
}


void
SyncRecordReader::
init()
{
    try {
	// inputStream.init();
	for (;;) {
	    const Sample* samp;

            if (inputStream)
            {
                samp = inputStream->readSample();
            }
            else
            {
                samp = nextSample();
            }
	    // read/parse SyncRec header, full out variables list

            if (! samp)
            {
                headException = new SyncRecHeaderException
                    ("EOF before header sample received");
                break;
            }
	    if (samp->getId() == SYNC_RECORD_HEADER_ID)
            {
                if (_debug)
		    cerr << "received SYNC_RECORD_HEADER_ID" << endl;
		scanHeader(samp);
		samp->freeReference();
		break;
	    }
	    else
            {
                DLOG(("looking for record header ID ") << SYNC_RECORD_HEADER_ID
                     << ", but got: " << samp->getId());
                samp->freeReference();
            }
	}
    }
    catch(const n_u::IOException& e) {
        headException = new SyncRecHeaderException(e.what());
    }
}

SyncRecordReader::~SyncRecordReader()
{
    _qcond.lock();
    while (! _syncRecords.empty())
    {
        const Sample* sample = _syncRecords.front();
        _syncRecords.pop_front();
        sample->freeReference();
    }
    _qcond.unlock();

    list<SampleTag*>::iterator si;
    for (si = sampleTags.begin(); si != sampleTags.end(); ++si)
	delete *si;
    delete headException;
}

/* local map class with a destructor which cleans up any
 * pointers left hanging.
 */
namespace {
class LocalVarMap
{
public:
    LocalVarMap(): _map() {}
    ~LocalVarMap() {
	map<string,SyncRecordVariable*>::const_iterator vi;
	for (vi = _map.begin(); vi != _map.end(); ++vi) {
	    SyncRecordVariable* var = vi->second;
	    delete var;
	}
    }
    map<string,SyncRecordVariable*>& getMap() { return _map; }
private:
    map<string,SyncRecordVariable*> _map;
};
}

void SyncRecordReader::scanHeader(const Sample* samp) throw()
{
    size_t offset = 0;
    size_t lagoffset = 0;
    delete headException;
    headException = 0;

    startTime = (time_t)(samp->getTimeTag() / USECS_PER_SEC);

    _header = string((const char*)samp->getConstVoidDataPtr(),
                     samp->getDataLength());

    istringstream header(_header);

    if (_debug)
        cerr << "header=\n" << string((const char*)samp->getConstVoidDataPtr(),
		samp->getDataLength()) << endl;

    try {
        readKeyedQuotedValues(header);
    }
    catch (const SyncRecHeaderException& e) {
        headException = new SyncRecHeaderException(e);
	return;
    }

    string tmpstr;
    string section = "variables";
    header >> tmpstr;
    if (header.eof() || tmpstr != "variables") {
    	headException = new SyncRecHeaderException("variables {",
	    tmpstr);
	return;
    }

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr != "{") {
    	headException = new SyncRecHeaderException("variables {",
	    string("variables ") + tmpstr);
	return;
    }

    list<const SyncRecordVariable*> newvars;
    LocalVarMap varmap;
    map<string,SyncRecordVariable*>& vmap = varmap.getMap();

    map<string,SyncRecordVariable*>::const_iterator vi;
    list<const SyncRecordVariable*>::const_iterator vli;

    for (;;) {

	string vname;
	header >> vname;

        if (_debug)
	    cerr << "vname=" << vname << endl;

	if (header.eof()) goto eof;

	// look for end paren
	if (vname == "}") break;

	string vtypestr;
	int vlen;

	header >> vtypestr >> vlen;
        if (_debug) {
	    cerr << "vtypestr=" << vtypestr << endl;
	    cerr << "vlen=" << vlen << endl;
        }
	if (header.eof()) goto eof;

	// screen bad variable types here
	if (vtypestr.length() != 1) {
	    headException = new SyncRecHeaderException(
	    	string("unexpected variable type: ") + vtypestr);
	    return;
	}

	Variable::type_t vtype = Variable::CONTINUOUS;
	switch (vtypestr[0]) {
	case 'n':
	    vtype = Variable::CONTINUOUS;
	    break;
	case 'c':
	    vtype = Variable::COUNTER;
	    break;
	case 't':
	    vtype = Variable::CLOCK;	// shouldn't happen
	    break;
	case 'o':
	    vtype = Variable::OTHER;	// shouldn't happen
	    break;
	default:
	    headException = new SyncRecHeaderException(
	    	string("unexpected variable type: ") + vtypestr);
	    return;
	}

	string vunits = getQuotedString(header);
        if (_debug)
	    cerr << "vunits=" << vunits << endl;
	if (header.eof()) goto eof;

	string vlongname = getQuotedString(header);
        if (_debug)
	    cerr << "vlongname=\"" << vlongname << "\"" << endl;
	if (header.eof()) goto eof;


        // read calibration coefs, or if there is a calibration file, the
        // content will be: file="something" path="something"
	vector<float> coefs;
        CalFile* cfile = 0;

	for (;;) {
	    float val;
	    header >> val;
	    if (header.fail()) break;
            if (_debug)
	        cerr << "val=" << val << endl;
	    coefs.push_back(val);
	}
	if (header.eof()) goto eof;
        header.clear();

	// converted units
	string cunits = getQuotedString(header);
        if (_debug)
	    cerr << "cunits=" << vunits << endl;
	if (header.eof()) goto eof;

	char nextchar = 0;
	for (;;) {
	    header >> nextchar;
	    if (header.eof()) goto eof;
	    if (!::isspace(nextchar)) break;
	}

        if (nextchar == 'f') {
            // looks like a file=" indicating a calibration file.
            vector<char> text(5,'\0');
            text[0] = nextchar;

            header.getline(&text.front()+1,text.size()-1,'=');
            if (header.eof()) goto eof;

            text.resize(header.gcount()+1,'\0');
            if (_debug) cerr << "read: " << &text.front() << endl;
            if (string(&text.front()) != "file")
                throw SyncRecHeaderException("file=",string(&text.front()));
            header.clear();
            string calName = getQuotedString(header);
            if (_debug) cerr << "calName=" << calName << endl;

            for (;;) {
                header >> nextchar;
                if (header.eof()) goto eof;
                if (!::isspace(nextchar)) break;
            }

            if (nextchar == 'p') {
                text.resize(5,'\0');
                text[0] = nextchar;
                header.getline(&text.front()+1,text.size()-1,'=');
                if (header.eof()) goto eof;
                text.resize(header.gcount()+1,'\0');
                if (_debug) cerr << "read: " << &text.front() << endl;
                if (string(&text.front()) != "path")
                    throw SyncRecHeaderException("path=",&text.front());
                header.clear();
                string calPath = getQuotedString(header);
                if (_debug) cerr << "calPath=" << calPath << endl;

                cfile = new CalFile();
                cfile->setFile(calName);
                cfile->setPath(calPath);
                for (;;) {
                    header >> nextchar;
                    if (header.eof()) goto eof;
                    if (!::isspace(nextchar)) break;
                }
            }
        }

        // final char should be a semicolon
	if (nextchar != ';') {
	    headException =
	    	new SyncRecHeaderException(";",string(nextchar,1));
	    return;
	}

	// variable fields parsed, now create SyncRecordVariable.
	SyncRecordVariable* var = new SyncRecordVariable();

	var->setName(vname);

	var->setType(vtype);

	var->setLength(vlen);
	var->setUnits(vunits);
	var->setLongName(vlongname);

        if (!coefs.empty() || cfile) {
            Polynomial* poly = new Polynomial();
            poly->setCoefficients(coefs);
            poly->setUnits(cunits);
            poly->setCalFile(cfile);
            var->setConverter(poly);
        }
	
	vmap[vname] = var;
	newvars.push_back(var);
    }
    if (_debug)
        cerr << "done with variable loop" << endl;
    section = "rates";

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr != "rates") {
    	headException = new SyncRecHeaderException("rates {",
	    tmpstr);
	return;
    }

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr != "{") {
    	headException = new SyncRecHeaderException("rates {",
	    string("rates ") + tmpstr);
        return;
    }

    // read rate groups
    for (;;) {
	float rate;
        header >> rate;
        if (_debug)
	    cerr << "read rate=" << rate << endl;
	if (header.fail()) break;
	if (header.eof()) goto eof;

	size_t groupSize = 1;	// number of data values in a group
	offset++;		// first value in group is the lag value

	SampleTag* stag = new SampleTag();
	sampleTags.push_back(stag);
	stag->setRate(rate);

	// read variable names of this rate
	for (;;) {
	    string vname;
	    header >> vname;
            if (_debug)
	        cerr << "variable of rate: " << vname << endl;
	    if (header.eof()) goto eof;
	    if (vname == ";") break;
	    vi = vmap.find(vname);
	    if (vi == vmap.end()) {
		ostringstream ost;
		ost << rate;
	        headException = new SyncRecHeaderException(
			string("variable ") + vname +
			" not found for rate " + ost.str());
		return;
	    }
	    SyncRecordVariable* var = vi->second;
	    if (!var) {
		ostringstream ost;
		ost << rate;
	        headException = new SyncRecHeaderException(
			string("variable ") + vname +
			" listed under more than one rate " + ost.str());
		return;
	    }

	    stag->addVariable(var);
	    var->setSyncRecOffset(offset);
	    var->setLagOffset(lagoffset);
	    int ndata = var->getLength() * (int)ceil(rate);
	    offset += ndata;
	    groupSize += ndata;
	    vmap[vname] = 0;
	}
	lagoffset += groupSize;
    }

    header.clear();	// clear fail bit
    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr != "}")
	headException = new SyncRecHeaderException("}",
	    tmpstr);

    // check that all variables have been found in a rate group
    for (vi = vmap.begin(); vi != vmap.end(); ++vi) {
        SyncRecordVariable* var = vi->second;
	if (var) {
	    headException = new SyncRecHeaderException(
	    	string("variable ") + var->getName() +
		" not found in a rate group");
            if (_debug)
	        cerr << headException->what() << endl;
	    return;
	}
    }

    variables = newvars;

    // make the variableMap for quick lookup.
    for (vli = variables.begin(); vli != variables.end(); ++vli) {
	const SyncRecordVariable* varp = *vli;
    	variableMap[varp->getName()] = varp;
    }

    numDataValues = offset;
    return;

eof: 
    headException = new SyncRecHeaderException(section,"end of header");
    return;
}

string SyncRecordReader::getQuotedString(istringstream& istr)
{
    istr.ignore(std::numeric_limits<int>::max(),'"');
    if (istr.fail()) return "";
    char val[512];
    istr.getline(val,sizeof(val),'"');
    return val;
}

void SyncRecordReader::readKeyedQuotedValues(istringstream& header)
	throw(SyncRecHeaderException)
{

    for (;;) {
        string key,value;
        header >> key;
        if (header.eof())
            throw SyncRecHeaderException("end of header when reading keyed values");
        if (key.length() > 0 && key[0] == '#') break;

        value = getQuotedString(header);

        if (header.eof())
            throw SyncRecHeaderException(string("end of header when reading value for key=") +
		key);
        if (header.fail())
            throw SyncRecHeaderException(string("failure reading quoted field from header for key=") +
		key);

        if (key == "project") projectName = value;
        else if (key == "aircraft") aircraftName = value;
        else if (key == "flight") flightName = value;
        else if (key == "software_version") softwareVersion = value;
    }
}

size_t SyncRecordReader::read(dsm_time_t* tt,double* dest,size_t len) 
    throw(n_u::IOException)
{
    for (;;) {
        // The next sample either comes from the SampleInputStream or from
        // the received queue.
        const Sample* samp = 0;
        if (inputStream)
        {
            samp = inputStream->readSample();
        }
        else if (syncServer && _read_sync_server)
        {
            /* This does not work, server runs in thread instead.  See
             * comment in constructor.
             */
            try {
                syncServer->read(true);
            }
            catch (n_u::IOException& ioe) {
                // Thus ends the feed of sync samples, and the intervening
                // SamplePipeline should have been flush()ed.  Fortunately,
                // that happens synchronously, so we should have received
                // in the queue all the samples we're going to get.  We're
                // completely done once the queue is empty.
                delete syncServer;
                syncServer = 0;
            }
            samp = nextSample();
        }
        else
        {
            // We're waiting on samples to arrive in the receive queue
            // from a SyncServer thread.
            samp = nextSample();
        }
        if (samp)
        {
            if (samp->getId() == SYNC_RECORD_ID)
            {
                assert(samp->getType() == DOUBLE_ST);
                if (len > samp->getDataLength()) 
                    len = samp->getDataLength();
                *tt = samp->getTimeTag();
                memcpy(dest, samp->getConstVoidDataPtr(), len*sizeof(double));
                samp->freeReference();
                return len;
            }
            else
            {
                samp->freeReference();
                continue;
            }
        }
        // The only way to get here is if our inputs are exhausted and no
        // more samples are available in the queue.
        if (!inputStream && !_read_sync_server)
        {
            throw n_u::EOFException("SyncServer", "read");
        }
    }
}


const list<const SyncRecordVariable*> 
SyncRecordReader::
getVariables() throw(n_u::Exception)
{
    if (headException) throw *headException;
    return variables;
}


const SyncRecordVariable* 
SyncRecordReader::
getVariable(const std::string& name) const
{
    map<string,const SyncRecordVariable*>::const_iterator vi;
    vi = variableMap.find(name);
    if (vi == variableMap.end()) return 0;
    return vi->second;
}


const Sample*
SyncRecordReader::
nextSample()
{
    // If there is a Sample in the queue, return it.  If there is no
    // SyncServer reference from which we can explicitly read more samples,
    // then we must wait on a signal that a sample has been added.
    const Sample* sample = 0;
    _qcond.lock();
    if (!_read_sync_server)
    {
        while (!_eoq && _syncRecords.empty())
        {
            _qcond.wait();
        }
    }
    if (!_syncRecords.empty())
    {
        sample = _syncRecords.front();
        _syncRecords.pop_front();
        // Signal back to the receive() thread that the queue has gone
        // down, in case it is waiting for the queue to drop below a
        // threshold.
        _qcond.signal();
    }
    _qcond.unlock();
    return sample;
}


bool
SyncRecordReader::
receive(const Sample *samp) throw()
{
    _qcond.lock();
    samp->holdReference();
    _syncRecords.push_back(samp);
    _qcond.signal();

    // Limit the number of samples in the buffer.  I believe holding up
    // this method will suspend the SyncRecordSource, then the pipeline
    // will hold up because its samples are not being read.
    while (_syncRecords.size() > 60)
    {
        _qcond.wait();
    }        
    _qcond.unlock();
    return true;
}


void
SyncRecordReader::
flush() throw()
{


}


void
SyncRecordReader::
endOfStream()
{
    DLOG(("SyncRecordReader::endOfStream()"));
    _qcond.lock();
    _eoq = true;
    _qcond.signal();
    _qcond.unlock();
}


int
SyncRecordReader::
getLagOffset(const nidas::core::Variable* var) throw (SyncRecHeaderException)

{ 
#ifdef notdef
    // If we have a SyncServer reference, use it to get the sync record lag
    // for this variable directly from the SyncRecordSource.  The idea is
    // to avoid relying on the sync record header if at all possible.
    // Otherwise get the lag from the local SyncRecordVariable list.

    // For now, though, just shortcut to the local SyncRecordVariable
    // derived from the header.
    if (syncServer)
    {
        SyncRecordSource* source = syncServer->getSyncRecordSource();
        return source->getLagOffset(var);
    }
#endif
    const SyncRecordVariable* sv = getVariable(var->getName());
    if (!sv)
    {
        std::ostringstream msg;
        msg << "no such variable " << var->getName() << " in getLagOffset()";
        throw SyncRecHeaderException(msg.str());
    }
    return sv->getLagOffset();
}


int
SyncRecordReader::
getSyncRecOffset(const nidas::core::Variable* var)
    throw (SyncRecHeaderException)
{
    const SyncRecordVariable* sv = getVariable(var->getName());
    if (!sv)
    {
        std::ostringstream msg;
        msg << "no such variable " << var->getName() 
            << " in getSyncRecOffset()";
        throw SyncRecHeaderException(msg.str());
    }
    return sv->getSyncRecOffset();
}

