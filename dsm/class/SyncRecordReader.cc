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
	inputStream(iochan),headException(0),ioException(0),
	numFloats(0),eof(false)
{
    inputStream.init();
    inputStream.addSampleClient(this);

    blockSignal(SIGHUP);
    blockSignal(SIGINT);
    blockSignal(SIGTERM);
    start();

    varCond.lock();
    while (variables.size() == 0) varCond.wait();
    varCond.unlock();
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
	eof = true;
    }
    catch (const atdUtil::IOException& e) {
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
	// cerr << "received SYNC_RECORD_HEADER_ID" << endl;
	atdUtil::Synchronized autolock(varCond);
	if (variables.size() == 0) {
	    scanHeader(samp);
	    varCond.signal();
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

    list<const SyncRecordVariable*> newvars;
    map<string,SyncRecordVariable*> varmap;
    map<string,SyncRecordVariable*>::const_iterator vi;

    string tmpstr;

    string section("project");
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("project")) {
    	headException = new SyncRecHeaderException("\"project {\"",
	    tmpstr);
	goto except;
    }
    header >> projectName;
    if (header.eof()) {
    	headException = new SyncRecHeaderException("\"project {\"",
	    projectName);
	goto except;
    }
    tmpstr.clear();

    section = "aircraft";
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("aircraft")) {
    	headException = new SyncRecHeaderException("\"aircraft {\"",
	    tmpstr);
	goto except;
    }
    header >> aircraftName;
    if (header.eof()) {
    	headException = new SyncRecHeaderException("\"aircraft {\"",
	    aircraftName);
	goto except;
    }
    tmpstr.clear();

    section = "variables";
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("variables")) {
    	headException = new SyncRecHeaderException("\"variables {\"",
	    tmpstr);
	goto except;
    }

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("{")) {
    	headException = new SyncRecHeaderException("\"variables {\"",
	    string("variables ") + tmpstr);
	goto except;
    }

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
	    goto except;
	}

	// variable fields parsed, now create SyncRecordVariable.
	SyncRecordVariable* var = new SyncRecordVariable();

	var->setName(vname);

	Variable::type_t vtype = Variable::CONTINUOUS;
	if (vtypestr.length() == 1) {
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
	    	throw new SyncRecHeaderException("variable type",vtypestr);
	    }
	}
	var->setType(vtype);

	var->setLength(vlen);
	var->setUnits(vunits);
	var->setLongName(vlongname);

	if (coefs.size() == 2) {
	    Linear* linear = new Linear();
	    linear->setSlope(coefs[0]);
	    linear->setIntercept(coefs[1]);
	    var->setConverter(linear);
	}
	else if (coefs.size() > 2) {
	    Polynomial* poly = new Polynomial();
	    poly->setCoefficients(coefs);
	    var->setConverter(poly);
	}
	
	varmap[vname] = var;
	newvars.push_back(var);
	// cerr << "var=" << var->getName() <<  endl;
    }
    section = "rates";

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("rates"))
    	headException = new SyncRecHeaderException("\"rates {\"",
	    tmpstr);

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("{"))
    	headException = new SyncRecHeaderException("\"rates {\"",
	    string("rates ") + tmpstr);

    // read rate groups
    for (;;) {
	float rate;
        header >> rate;
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
	    if (header.eof()) goto eof;
	    if (!vname.compare(";")) break;
	    SyncRecordVariable* var = varmap[vname];
	    if (!var) {
		ostringstream ost;
		ost << rate;
	        headException = new SyncRecHeaderException(
			string("cannot find variable ") + vname +
			" for rate " + ost.str());
		goto except;
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
	    goto except;
	}
    }

    variables = newvars;
    numFloats = offset;
    goto cleanup;

eof: 
    headException = new SyncRecHeaderException(section,"end of header");
except:
cleanup:
    for (vi = varmap.begin(); vi != varmap.end(); ++vi) {
        SyncRecordVariable* var = vi->second;
	if (var) delete var;
    }
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

