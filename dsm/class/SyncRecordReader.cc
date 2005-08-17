/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-06-06 10:45:06 -0600 (Mon, 06 Jun 2005) $

    $LastChangedRevision: 2222 $

    $LastChangedBy: maclean $

    $HeadURL: http://localhost:8080/svn/hiaper/ads3/dsm/class/SampleInput.cc $
 ********************************************************************
*/

#include <SyncRecordReader.h>
#include <SyncRecordSource.h>
#include <atdUtil/EOFException.h>

using namespace dsm;
using namespace std;

SyncRecordReader::SyncRecordReader(IOChannel*iochan):
	atdUtil::Thread(string("SyncRecordReader ") + iochan->getName()),
	inputStream(iochan),headerScanned(false),headException(0),
	ioException(0),numFloats(0),eof(false)
{
    inputStream.init();
    inputStream.addSampleClient(this);

    blockSignal(SIGHUP);
    blockSignal(SIGINT);
    blockSignal(SIGTERM);
    start();

    headerCond.lock();
    while (!headerScanned) headerCond.wait();
    headerCond.unlock();
}

SyncRecordReader::~SyncRecordReader()
{
    if (isRunning()) cancel();
    if (!isJoined()) join();
    list<SampleTag*>::iterator si;
    for (si = sampleTags.begin(); si != sampleTags.end(); ++si)
	delete *si;
    delete ioException;
    delete headException;
}

int SyncRecordReader::run() throw(atdUtil::Exception) {
    try {
	for(;;) {
	    if (isInterrupted()) break;
	    inputStream.readSamples();
	}
	throw atdUtil::IOException("SyncRecordReader","read",EINTR);
    }
    catch (const atdUtil::EOFException& e) {
	cerr << e.what() << endl;
	eof = true;
    }
    catch (const atdUtil::IOException& e) {
	cerr << e.what() << endl;
    	ioException = new atdUtil::IOException(e);
    }
    cerr << "SyncRecordHeader::run finished" << endl;
    syncRecSem.post();
    return 0;
}


bool SyncRecordReader::receive(const Sample* samp) throw()
{
    // read/parse SyncRec header, full out variables list
    if (samp->getId() == SYNC_RECORD_HEADER_ID) {
	cerr << "received SYNC_RECORD_HEADER_ID" << endl;
	atdUtil::Synchronized autolock(headerCond);
	if (!headerScanned) {
	    scanHeader(samp);
	    headerScanned = true;
	    headerCond.signal();
	}
	return true;
    }
    else if (samp->getId() == SYNC_RECORD_ID) {
	// cerr << "received SYNC_RECORD_ID" << endl;

	// This thread is the only one changing variables, so
	// it's OK to access it without a mutex.
	if (variables.size() == 0) return false;

	samp->holdReference();

	syncRecCond.lock();
	// don't get too far ahead of the other thread
	while (syncRecs.size() > 5) syncRecCond.wait();
        syncRecs.push_back(samp);

#ifdef DEBUG
	if ((unsigned)syncRecSem.getValue() != syncRecs.size())
	    cerr << "syncRecSem.getValue()=" << syncRecSem.getValue() <<
		" syncRecs.size()=" << syncRecs.size() << endl;
#endif
	    
	syncRecCond.unlock();
	syncRecSem.post();

	return true;
    }
    return false;
}

/* local map class with a destructor which cleans up any
 * pointers left hanging.
 */
namespace {
class LocalVarMap: public map<string,SyncRecordVariable*>
{
public:
    ~LocalVarMap() {
	map<string,SyncRecordVariable*>::const_iterator vi;
	for (vi = begin(); vi != end(); ++vi) {
	    SyncRecordVariable* var = vi->second;
	    if (var) delete var;
	}
    }
};
}

void SyncRecordReader::scanHeader(const Sample* samp) throw()
{
    size_t offset = 0;
    size_t lagoffset = 0;
    delete headException;
    headException = 0;

    istringstream header(
    	string((const char*)samp->getConstVoidDataPtr(),
		samp->getDataLength()));

    cerr << "header=\n" << 
    	string((const char*)samp->getConstVoidDataPtr(),
		samp->getDataLength()) << endl;

    try {
        projectName = getKeyedValue(header,"project");
        aircraftName = getKeyedValue(header,"aircraft");
        flightName = getKeyedValue(header,"flight");
    }
    catch (const SyncRecHeaderException& e) {
        headException = new SyncRecHeaderException(e);
	return;
    }

    string tmpstr;
    string section = "variables";
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("variables")) {
    	headException = new SyncRecHeaderException("variables {",
	    tmpstr);
	return;
    }

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("{")) {
    	headException = new SyncRecHeaderException("variables {",
	    string("variables ") + tmpstr);
	return;
    }

    list<const SyncRecordVariable*> newvars;
    LocalVarMap varmap;
    map<string,SyncRecordVariable*>::const_iterator vi;

    for (;;) {

	string vname;
	header >> vname;

	// cerr << "vname=" << vname << endl;

	if (header.eof()) goto eof;

	// look for end paren
	if (!vname.compare("}")) break;

	string vtypestr;
	int vlen;

	header >> vtypestr >> vlen;
	// cerr << "vtypestr=" << vtypestr << endl;
	// cerr << "vlen=" << vlen << endl;
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
	// cerr << "vunits=" << vunits << endl;
	if (header.eof()) goto eof;

	string vlongname = getQuotedString(header);
	// cerr << "vlongname=" << vlongname << endl;
	if (header.eof()) goto eof;

	vector<float> coefs;

	for (;;) {
	    float val;
	    header >> val;
	    if (header.fail()) break;
	    // cerr << "val=" << val << endl;
	    coefs.push_back(val);
	}
	if (header.eof()) goto eof;
	header.clear();

	char semicolon = 0;
	for (;;) {
	    header >> semicolon;
	    if (header.eof()) goto eof;
	    if (semicolon != ' ' && semicolon != '\t' &&
	    	semicolon != '\n' && semicolon != '\r')
	    	break;
	}

	if (semicolon != ';') {
	    headException =
	    	new SyncRecHeaderException(";",string(semicolon,1));
	    return;
	}

	// variable fields parsed, now create SyncRecordVariable.
	SyncRecordVariable* var = new SyncRecordVariable();

	var->setName(vname);

	var->setType(vtype);

	var->setLength(vlen);
	var->setUnits(vunits);
	var->setLongName(vlongname);

	if (coefs.size() == 2) {
	    Linear* linear = new Linear();
	    linear->setIntercept(coefs[0]);
	    linear->setSlope(coefs[1]);
	    var->setConverter(linear);
	}
	else if (coefs.size() > 2) {
	    Polynomial* poly = new Polynomial();
	    poly->setCoefficients(coefs);
	    var->setConverter(poly);
	}
	
	varmap[vname] = var;
	newvars.push_back(var);
    }
    cerr << "done with variable loop" << endl;
    section = "rates";

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("rates")) {
    	headException = new SyncRecHeaderException("rates {",
	    tmpstr);
	return;
    }

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("{")) {
    	headException = new SyncRecHeaderException("rates {",
	    string("rates ") + tmpstr);
        return;
    }

    // read rate groups
    for (;;) {
	float rate;
        header >> rate;
	cerr << "read rate=" << rate << endl;
	if (header.fail()) break;
	if (header.eof()) goto eof;

	size_t groupSize = 1;	// number of float values in a group
	offset++;		// first float in group is the lag value

	SampleTag* stag = new SampleTag();
	sampleTags.push_back(stag);
	stag->setRate(rate);

	// read variable names of this rate
	for (;;) {
	    string vname;
	    header >> vname;
	    cerr << "variable of rate: " << vname << endl;
	    if (header.eof()) goto eof;
	    if (!vname.compare(";")) break;
	    vi = varmap.find(vname);
	    if (vi == varmap.end()) {
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
	    int nfloat = var->getLength() * (int)ceil(rate);
	    offset += nfloat;
	    groupSize += nfloat;
	    varmap[vname] = 0;
	}
	lagoffset += groupSize;
    }
    cerr << "out of rate loop" << endl;
    header.clear();	// clear fail bit
    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("}"))
	headException = new SyncRecHeaderException("}",
	    tmpstr);

    // check that all variables have been found in a rate group
    for (vi = varmap.begin(); vi != varmap.end(); ++vi) {
        SyncRecordVariable* var = vi->second;
	if (var) {
	    headException = new SyncRecHeaderException(
	    	string("variable ") + var->getName() +
		" not found in a rate group");
	    cerr << headException->what() << endl;
	    return;
	}
    }

    variables = newvars;
    numFloats = offset;
    cerr << "scanHeader done" << endl;
    return;

eof: 
    headException = new SyncRecHeaderException(section,"end of header");
    return;
}

string SyncRecordReader::getQuotedString(istringstream& istr)
{
    string val;

    char dquote = 0;
    do {
	istr >> dquote;
	if (istr.eof()) return val;
    } while (dquote != '\"');


    for (;;) {
	istr >> dquote;
	if (istr.eof() || dquote == '\"') break;
	val += dquote;
    }
    return val;
}

string SyncRecordReader::getKeyedValue(istringstream& header,const string& key)
	throw(SyncRecHeaderException)
{

    string tmpstr;
    header >> tmpstr;
    if (header.eof())
    	throw SyncRecHeaderException(string("end of header when reading ") +
		key);

    if (tmpstr.compare("key"))
    	throw SyncRecHeaderException("key",tmpstr);

    header >> tmpstr;
    if (header.eof())
    	throw SyncRecHeaderException(string("end of header when reading value for ") +
		key);
    return tmpstr;
}

size_t SyncRecordReader::read(dsm_time_t* tt,float* dest,size_t len) throw(atdUtil::IOException)
{
    // wait for semaphore from thread that is putting samples into syncRecs
    // If we get a signal, this wait will return, but there will be
    // no data.
    for (;;) {
	syncRecSem.wait();
	const Sample* samp = 0;
	{
	    atdUtil::Synchronized autolock(syncRecCond);
	    if (syncRecs.size() == 0) {
		if (!eof && ioException == 0) continue;	// no data, wait again
		if (eof) return 0;
		throw *ioException;
	    }
	    samp = syncRecs.front();
	    syncRecs.pop_front();
	    syncRecCond.signal();
	}

	// cerr << "len=" << len << " samp->getDataLength=" << samp->getDataLength() << endl;
	if (len > samp->getDataLength()) len = samp->getDataLength();
	// cerr << "len=" << len << endl;

	*tt = samp->getTimeTag();
	memcpy(dest,samp->getConstVoidDataPtr(),len * sizeof(float));

	samp->freeReference();

	return len;
    }
}

const list<const SyncRecordVariable*> SyncRecordReader::getVariables() throw(atdUtil::Exception)
{
    if (headException) throw *headException;
    return variables;
}

