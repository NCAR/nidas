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

using namespace dsm;
using namespace std;

SyncRecordReader::SyncRecordReader(): exception(0)
{
}

SyncRecordReader::~SyncRecordReader()
{
    list<SampleTag*>::iterator si;
    for (si = sampleTags.begin(); si != sampleTags.end(); ++si)
	delete *si;
    delete exception;
}
void SyncRecordReader::connect(SampleInput* input)
	throw(atdUtil::IOException)
{
    input->addSampleClient(this);
}

void SyncRecordReader::disconnect(SampleInput* input)
	throw(atdUtil::IOException)
{
    input->removeSampleClient(this);
    sem_post(&readSem);
}

bool SyncRecordReader::receive(const Sample* samp) throw()
{
    // read/parse SyncRec header, full out variables list
    if (samp->getId() == SYNC_RECORD_HEADER_ID) {
        scanHeader(samp);
	return true;
    }
    else if (samp->getId() == SYNC_RECORD_ID) {
	samp->holdReference();
        syncRecs.push_back(samp);
	sem_post(&readSem);
	return true;
    }
    return false;
}

void SyncRecordReader::scanHeader(const Sample* samp) throw()
{
    delete exception;
    exception = 0;

    istringstream header(
    	string((const char*)samp->getConstVoidDataPtr(),
		samp->getDataLength()));

    list<const SyncRecordVariable*> newvars;
    map<string,SyncRecordVariable*> varmap;
    map<string,SyncRecordVariable*>::const_iterator vi;
    string section("variables");

    string tmpstr;

    header >> tmpstr;
    if (header.eof() || tmpstr.compare("variables")) {
    	exception = new SyncRecHeaderException("\"variables {\"",
	    tmpstr);
	goto except;
    }

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("{")) {
    	exception = new SyncRecHeaderException("\"variables {\"",
	    string("variables ") + tmpstr);
	goto except;
    }

    for (;;) {

	string vname;
	header >> vname;

	if (header.eof()) goto eof;

	// look for end paren
	if (!vname.compare("}")) break;

	string vtypestr;
	int vlen;

	header >> vtypestr >> vlen;
	if (header.eof()) goto eof;

	string vunits = getQuotedString(header);
	if (header.eof()) goto eof;

	string vlongname = getQuotedString(header);
	if (header.eof()) goto eof;

	vector<float> coefs;

	for (;;) {
	    float val;
	    header >> val;
	    if (header.fail()) break;
	    coefs.push_back(val);
	}
	if (header.eof()) goto eof;

	char semicolon = 0;
	for (;;) {
	    header >> semicolon;
	    if (header.eof()) goto eof;
	    if (semicolon != ' ' && semicolon != '\t' &&
	    	semicolon != '\n' && semicolon != '\r')
	    	break;
	}

	if (semicolon != ';') {
	    exception =
	    	new SyncRecHeaderException("semicolon",string(semicolon,1));
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
    }
    section = string("variables");

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("rates"))
    	exception = new SyncRecHeaderException("\"rates {\"",
	    tmpstr);

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("{"))
    	exception = new SyncRecHeaderException("\"rates {\"",
	    string("rates ") + tmpstr);

    // read rate groups
    for (;;) {
	float rate;
        header >> rate;
	if (header.fail()) break;
	if (header.eof()) goto eof;
	SampleTag* stag = new SampleTag();
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
	        exception = new SyncRecHeaderException(
			string("cannot find variable ") + vname +
			" for rate " + ost.str());
		goto except;
	    }

	    stag->addVariable(var);
	    varmap[vname] = 0;
	}
    }
    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("}"))
	exception = new SyncRecHeaderException("}",
	    tmpstr);

    // check that all variables have been found in a rate group
    for (vi = varmap.begin(); vi != varmap.end(); ++vi) {
        SyncRecordVariable* var = vi->second;
	if (var) {
	    exception = new SyncRecHeaderException(
	    	string("variable ") + var->getName() +
		" not found in a rate group");
	    goto except;
	}
    }

    {
	atdUtil::Synchronized autolock(varsMutex);
	variables = newvars;
    }
    goto cleanup;

eof: 
    exception = new SyncRecHeaderException(section,"end of header");
except:
cleanup:
    sem_post(&varsSem);
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

size_t SyncRecordReader::read(float* dest,size_t len) throw()
{
    sem_wait(&readSem);
    const Sample* samp = 0;

    {
        atdUtil::Synchronized autolock(recsMutex);

	if (syncRecs.size() == 0) return 0;
	samp = syncRecs.front();
	syncRecs.pop_front();
    }

    if (len < samp->getDataLength()) len = samp->getDataLength();

    memcpy(dest,samp->getConstVoidDataPtr(),len * sizeof(float));

    samp->freeReference();

    return len;
}

const list<const SyncRecordVariable*> SyncRecordReader::getVariables() throw(atdUtil::Exception)
{
    {
	atdUtil::Synchronized autolock(varsMutex);
	if (exception) throw *exception;
	if (variables.size() > 0) return variables;
    }
    sem_wait(&varsSem);
    if (exception) throw *exception;
    return variables;
}

