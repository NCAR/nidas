// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
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

#ifdef HAS_NC_SERVER_RPC_H

#include <nidas/dynld/isff/NetcdfRPCChannel.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/Site.h>
#include <nidas/core/Project.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Process.h>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NetcdfRPCChannel::NetcdfRPCChannel():
    _name(),_server(),_fileNameFormat(),_directory(),_cdlFileName(),
    _fillValue(1.e37), _fileLength(SECS_PER_DAY),
    _clnt(0), _connectionId(0), _rpcBatchPeriod(300),
    _rpcWriteTimeout(),_rpcOtherTimeout(),_rpcBatchTimeout(),
    _ntry(0),_lastNonBatchWrite(0),
    _groupById(),_stationIndexById(),_groups(),_sampleTags(),
    _timeInterval(300)
{
    setName("NetcdfRPCChannel");
    setRPCTimeout(300);
    _rpcBatchTimeout.tv_sec = 0;
    _rpcBatchTimeout.tv_usec = 0;
}

/* copy constructor */
NetcdfRPCChannel::NetcdfRPCChannel(const NetcdfRPCChannel& x):
    IOChannel(x),
    _name(x._name),
    _server(x._server),
    _fileNameFormat(x._fileNameFormat),
    _directory(x._directory),
    _cdlFileName(x._cdlFileName),
    _fillValue(x._fillValue),
    _fileLength(x._fileLength),
    _clnt(0),_connectionId(0),
    _rpcBatchPeriod(x._rpcBatchPeriod),
    _rpcWriteTimeout(x._rpcWriteTimeout),
    _rpcOtherTimeout(x._rpcOtherTimeout),
    _rpcBatchTimeout(x._rpcBatchTimeout),
    _ntry(0),_lastNonBatchWrite(0),
    _groupById(),_stationIndexById(),_groups(),_sampleTags(),
    _timeInterval(x._timeInterval)
{
    _rpcBatchTimeout.tv_sec = 0;
    _rpcBatchTimeout.tv_usec = 0;
}

NetcdfRPCChannel::~NetcdfRPCChannel()
{
}

void NetcdfRPCChannel::addSampleTag(const SampleTag* val)
{
    if (find(_sampleTags.begin(),_sampleTags.end(),val) == _sampleTags.end())
        _sampleTags.push_back(val);
}

void NetcdfRPCChannel::setName(const std::string& val)
{
    _name = val;
}

void NetcdfRPCChannel::setServer(const string& val)
{
    _server = val;
    setName(string("ncserver: ") + getServer() + ':' + 
    	getDirectory() + "/" + getFileNameFormat());
}

void NetcdfRPCChannel::setFileNameFormat(const string& val)
{
    _fileNameFormat = val;
    setName(string("ncserver: ") + getServer() + ':' + 
    	getDirectory() + "/" + getFileNameFormat());
}

void NetcdfRPCChannel::setDirectory(const string& val)
{
    _directory = val;
    setName(string("ncserver: ") + getServer() + ':' + 
    	getDirectory() + "/" + getFileNameFormat());
}

void NetcdfRPCChannel::setRPCTimeout(int secs)
{
    _rpcWriteTimeout.tv_sec = secs;
    _rpcWriteTimeout.tv_usec = 0;
    _rpcOtherTimeout.tv_sec = secs * 5;
    _rpcOtherTimeout.tv_usec = 0;
}

int NetcdfRPCChannel::getRPCTimeout() const
{
    return _rpcWriteTimeout.tv_sec;
}

struct timeval& NetcdfRPCChannel::getRPCWriteTimeoutVal()
{
    return _rpcWriteTimeout;
}

struct timeval& NetcdfRPCChannel::getRPCOtherTimeoutVal()
{
    return _rpcOtherTimeout;
}

void NetcdfRPCChannel::setRPCBatchPeriod(int secs)
{
    _rpcBatchPeriod = secs;
}

int NetcdfRPCChannel::getRPCBatchPeriod() const
{
    return _rpcBatchPeriod;
}

void NetcdfRPCChannel::requestConnection(IOChannelRequester* rqstr)
       throw(n_u::IOException)
{
    connect();
    rqstr->connected(this);
}


IOChannel* NetcdfRPCChannel::connect()
       throw(n_u::IOException)
{
    // expand the file and directory names.

    if (getDSMConfig()) {
        setDirectory(getDSMConfig()->expandString(getDirectory()));
        setFileNameFormat(getDSMConfig()->expandString(getFileNameFormat()));
        setCDLFileName(getDSMConfig()->expandString(getCDLFileName()));
    }
    else {
        setDirectory(Project::getInstance()->expandString(getDirectory()));
        setFileNameFormat(Project::getInstance()->expandString(getFileNameFormat()));
        setCDLFileName(Project::getInstance()->expandString(getCDLFileName()));
    }

    _clnt = clnt_create(getServer().c_str(),
    	NETCDFSERVERPROG, NETCDFSERVERVERS,"tcp");
    if (_clnt == (CLIENT *) NULL)
        throw n_u::IOException(getName(),"clnt_create",
	    clnt_spcreateerror(_server.c_str()));

    connection conn;
    conn.filenamefmt = (char *)getFileNameFormat().c_str();

    conn.outputdir = (char *)getDirectory().c_str();
    conn.cdlfile = (char *)getCDLFileName().c_str();
    conn.filelength = getFileLength();
    conn.interval = getTimeInterval();

    int result = 0;
    enum clnt_stat clnt_stat;

    if ((clnt_stat = clnt_call(_clnt, OPENCONNECTION,
	  (xdrproc_t) xdr_connection, (caddr_t) &conn,
	  (xdrproc_t) xdr_int,  (caddr_t) &result,
	  _rpcOtherTimeout)) != RPC_SUCCESS) {
        n_u::IOException e(getName(),"open",
	    clnt_sperror(_clnt,_server.c_str()));
	clnt_destroy(_clnt);
	_clnt = 0;
	throw e;
    }

    _connectionId = result;
    if (_connectionId < 0) {
	clnt_destroy(_clnt);
	_clnt = 0;
	throw n_u::IOException(getName(),"open",
	  string("perhaps ") + _directory + " does not exist on server");
    }

    {
        ostringstream idstr;
        idstr << (_connectionId & 0xffff);
        setName(string("ncserver: ") + getServer() + ':' + 
            getDirectory() + "/" + getFileNameFormat() + ", id " + idstr.str());
    }

    _lastNonBatchWrite = time((time_t *)0);

    Project* project = Project::getInstance();

    unsigned int nstations = 0;
    if (project->getMaxSiteNumber() > 0)
	    nstations = project->getMaxSiteNumber() -
		project->getMinSiteNumber() + 1;
    set<int> stns;
    
    list<const SampleTag*> tags = getSampleTags();
    list<const SampleTag*>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        const SampleTag* stag = *si;

	// 0 for non-station, otherwise > 0
	int tagStation = stag->getStation();
	for (VariableIterator vi = stag->getVariableIterator();
		vi.hasNext(); ) {
	    const Variable* var = vi.next();

#ifdef DEBUG
	    cerr << "NetcdfRPCChannel::connect(), var=" << var->getName() <<
	    	" varstation=" << var->getStation() << 
		" tagstation=" << tagStation << endl;
#endif
	    int vstn = var->getStation();

	    if (vstn < 0) n_u::Logger::getInstance()->log(LOG_WARNING,
		"var %s is from station %d",
		var->getName().c_str(),vstn);

	    if (vstn > 0) stns.insert(vstn);
	    if (vstn != tagStation) n_u::Logger::getInstance()->log(LOG_WARNING,
		"var %s is from station %d, others in this sample are from %d",
		var->getName().c_str(),vstn,tagStation);
	}
	_stationIndexById[stag->getId()] = tagStation - 1;
    }

    vector<ParameterT<int> > dims;

    // if we have data from stations with value > 0
    if (stns.size() > 0) {
	if (stns.size() != nstations)
	    n_u::Logger::getInstance()->log(LOG_WARNING,
		"nstations=%d, stns.size()=%d",
		nstations,stns.size());

	ParameterT<int> stnDim;
	stnDim.setName("station");
	stnDim.setValue(nstations);
	dims.push_back(stnDim);
    }
        
    si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        const SampleTag* stag = *si;
	
	NcVarGroupFloat* grp = getNcVarGroupFloat(dims,stag);
	if (!grp) {
	    grp = new NcVarGroupFloat(dims,stag,_fillValue);
	    grp->connect(this,_fillValue);
	    _groups.push_back(grp);
	}
#ifdef DEBUG
	cerr << "adding to groupById, tag=" << stag->getId() << endl;
#endif
	_groupById[stag->getId()] = grp;
    }
    return this;
}

NcVarGroupFloat* NetcdfRPCChannel::getNcVarGroupFloat(
	const vector<ParameterT<int> >&dims,
	const SampleTag* stag)
{
    list<NcVarGroupFloat*>::const_iterator gi;
    for (gi = _groups.begin(); gi != _groups.end(); ++gi) {
        NcVarGroupFloat* grp = *gi;

	if (grp->getDimensions().size() != dims.size()) continue;
	vector<ParameterT<int> >::const_iterator di1;
	vector<ParameterT<int> >::const_iterator di2;
	for (di1 = grp->getDimensions().begin(),di2=dims.begin();
		di1 != grp->getDimensions().end(); ++di1,++di2) {
	    const ParameterT<int>& p1 = *di1;
	    const ParameterT<int>& p2 = *di2;
	    if (p1.getName() != p2.getName()) break;
	    if (p1.getLength() != 1) break;
	    if (p1.getLength() != p2.getLength()) break;
	    if (p1.getValue(0) != p2.getValue(0)) break;
	}
	if (di1 != grp->getDimensions().end()) continue;

	if (grp->getVariables().size() !=
		stag->getVariables().size()) continue;
	vector<const Variable*>::const_iterator vi1;
	vector<const Variable*>::const_iterator vi2;
	for (vi1 = grp->getVariables().begin(),
		vi2=stag->getVariables().begin();
		vi1 != grp->getVariables().end(); ++vi1,++vi2) {
	    const Variable* v1 = *vi1;
	    const Variable* v2 = *vi2;
	    if (!(*v1 == *v2)) break;
	}
	if (vi1 == grp->getVariables().end()) return grp;
    }
    return 0;
}

void NetcdfRPCChannel::write(const Sample* samp) 
    throw(n_u::IOException)
{

    dsm_sample_id_t sampid = samp->getId();

    map<dsm_sample_id_t,NcVarGroupFloat*>::const_iterator gi =
    	_groupById.find(sampid);

#ifdef DEBUG
    cerr << "NetcdfRPCChannel::write: " <<
    	n_u::UTime(samp->getTimeTag()).format(true,"%Y %m %d %H:%M:%S.%3f") << ' ' <<
    	GET_DSM_ID(sampid) << ',' << GET_SHORT_ID(sampid) << 
        " group.size=" << _groupById.size() << 
    	" found group=" << (gi != _groupById.end()) << endl;
#endif
    if (gi == _groupById.end()) return;

    NcVarGroupFloat* g = gi->second;

    int stationIndex = _stationIndexById[samp->getId()];
#ifdef DEBUG
    cerr << "NetcdfRPCChannel::write, stationIndex=" << stationIndex <<
    	endl;
#endif

    g->write(this,samp,stationIndex);
}

void NetcdfRPCChannel::write(datarec_float *rec) throw(n_u::IOException)
{

    /*
     * Every so often in batch mode check if nc_server actually responds.
     */
    if (_rpcBatchPeriod == 0 || time(0) - _lastNonBatchWrite > _rpcBatchPeriod) {
        nonBatchWrite(rec);
	return;
    }

    /*
     * For RPC batch mode, the timeout is set to 0.
     */
#ifdef DEBUG
    cerr << "NetcdfRPRChannel::write " <<
	n_u::UTime(rec->time).format(true,"%Y %m %d %H:%M:%S.%3f ") <<
	" id=" << rec->datarecId <<
	" v[0]=" << rec->data.data_val[0] << endl;
#endif
    enum clnt_stat clnt_stat;
    clnt_stat = clnt_call(_clnt, WRITEDATARECBATCH_FLOAT,
	(xdrproc_t) xdr_datarec_float, (caddr_t) rec,
	(xdrproc_t) NULL, (caddr_t) NULL,
	_rpcBatchTimeout);
    if (clnt_stat != RPC_SUCCESS)
	throw n_u::IOException(getName(),"write",clnt_sperror(_clnt,""));
}

void NetcdfRPCChannel::nonBatchWrite(datarec_float *rec) throw(n_u::IOException)
{
    int result = 0;
    enum clnt_stat clnt_stat;

    for ( ; ; ) {
	clnt_stat = clnt_call(_clnt, WRITEDATAREC_FLOAT,
	    (xdrproc_t) xdr_datarec_float, (caddr_t) rec,
	    (xdrproc_t) xdr_int, (caddr_t) &result,
	    _rpcWriteTimeout);
	if (clnt_stat != RPC_SUCCESS) {
	    bool serious = (clnt_stat != RPC_TIMEDOUT && clnt_stat != RPC_CANTRECV) ||
                _ntry++ >= NTRY;
	    if (serious) 
		throw n_u::IOException(getName(),"write", clnt_sperror(_clnt,""));
	    if (_ntry > NTRY / 2) {
		n_u::Logger::getInstance()->log(LOG_WARNING,
			"%s: %s, timeout=%d secs, ntry=%d",
		    getName().c_str(),clnt_sperror(_clnt,"nc_server not responding"),
		    _rpcWriteTimeout.tv_sec ,_ntry);
	    }
	}
	else {
	    if (!result && _ntry > 0)
	    	n_u::Logger::getInstance()->log(LOG_WARNING,"%s: OK",
		    getName().c_str());
            _ntry = 0;
            /* If result is non-zero, then an error occured on nc_server.
             * checkError() will retrieve the error string and throw the exception.
             */
            if (result) {
                checkError();
                // checkError should throw an exception if the above call returned a
                // negative result.  If not something's not working right.
                throw n_u::IOException(getName(),"write","unknown error");
            }
            _lastNonBatchWrite = time((time_t*)0);
            break;
        }
    }
}

void NetcdfRPCChannel::checkError() throw(n_u::IOException)
{

#ifdef DEBUG
    cerr << "NetcdfRPRChannel::checkError " <<
	n_u::UTime().format(true,"%Y %m %d %H:%M:%S.%3f ") << endl;
#endif

    char* errormsg = 0;
    enum clnt_stat clnt_stat = clnt_call(_clnt, CHECKERROR,
        (xdrproc_t) xdr_int, (caddr_t) &_connectionId,
        (xdrproc_t) xdr_wrapstring, (caddr_t) &errormsg,
        _rpcWriteTimeout);

    if (clnt_stat != RPC_SUCCESS) {
	bool serious = (clnt_stat != RPC_TIMEDOUT && clnt_stat != RPC_CANTRECV) ||
                _ntry++ >= NTRY;
        if (serious)
            throw n_u::IOException(getName(),"checkError",clnt_sperror(_clnt,""));
	n_u::Logger::getInstance()->log(LOG_WARNING,
		"%s: %s, timeout=%d secs, ntry=%d",
	    getName().c_str(),clnt_sperror(_clnt,"nc_server not responding"),
	    _rpcWriteTimeout.tv_sec ,_ntry);
    }
    else {
	if (!errormsg[0] && _ntry > 0)
	    n_u::Logger::getInstance()->log(LOG_WARNING,"%s: OK",
		getName().c_str());
	_ntry = 0;
	_lastNonBatchWrite = time((time_t*)0);

        /*
           If error string is non-empty, then an error occured on nc_server.
           The returned string must be freed.
        */
        if (errormsg[0]) {
            string msg = errormsg;
            xdr_free((xdrproc_t)xdr_wrapstring,(char*)&errormsg);
            throw n_u::IOException(getName(),"write",msg);
        }
        xdr_free((xdrproc_t)xdr_wrapstring,(char*)&errormsg);
    }
}

void NetcdfRPCChannel::close() throw(n_u::IOException)
{

    list<NcVarGroupFloat*>::const_iterator gi = _groups.begin();
    for ( ; gi != _groups.end(); ++gi) delete *gi;
    _groups.clear();
    _groupById.clear();

    if (_clnt) {
	int result = 0;
	enum clnt_stat clnt_stat;
	if ((clnt_stat = clnt_call(_clnt, CLOSECONNECTION,
	    (xdrproc_t) xdr_int, (caddr_t) &_connectionId,
	    (xdrproc_t) xdr_int, (caddr_t) &result,
	    _rpcOtherTimeout)) != RPC_SUCCESS) {
	  n_u::IOException e(getName(),"close",clnt_sperror(_clnt,""));
	  clnt_destroy(_clnt);
	  _clnt = 0;
	  throw e;
	}
	clnt_destroy(_clnt);
	_clnt = 0;
        ILOG(("closed: ") << getName());
    }
}

void NetcdfRPCChannel::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    if(node->hasAttributes()) {
	// get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	    if (aname == "server") setServer(n_u::Process::expandEnvVars(aval));
	    else if (aname == "dir") setDirectory(aval);
	    else if (aname == "file") setFileNameFormat(aval);
	    else if (aname == "cdl") setCDLFileName(aval);
	    else if (aname == "interval") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),
			aval, aval);
		setTimeInterval(val);
            }
	    else if (aname == "length") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),
			aval, aval);
		setFileLength(val);
	    }
	    else if (aname == "floatFill") {
		istringstream ist(aval);
		float val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),
			aname, aval);
		setFillValue(val);
	    }
	    else if (aname == "timeout") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),
			aval, aval);
		setRPCTimeout(val);
	    }
	    else if (aname == "batchPeriod") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),
			aval, aval);
		setRPCBatchPeriod(val);
	    }
	    else throw n_u::InvalidParameterException(getName(),
			"unrecognized attribute", aname);
	}
    }
}

NcVarGroupFloat::NcVarGroupFloat(
	const std::vector<ParameterT<int> >& dims,
        const SampleTag* stag,float fill):
	_dimensions(dims),
	_sampleTag(*stag),_rec(),
	_weightsIndex(-1),
	_fillValue(fill)
{
    _rec.start.start_val = 0;
    _rec.count.count_val = 0;
    _rec.cnts.cnts_val = 0;
    _rec.data.data_val = 0;
}

NcVarGroupFloat::~NcVarGroupFloat()
{
    delete [] _rec.start.start_val;
    delete [] _rec.count.count_val;
    delete [] _rec.cnts.cnts_val;
    delete [] _rec.data.data_val;
}

void NcVarGroupFloat::connect(NetcdfRPCChannel* conn,float _fillValue)
	throw(n_u::IOException)
{
    datadef ddef; 
    ddef.connectionId = conn->getConnectionId(); 
    ddef.rectype = NS_TIMESERIES;
    ddef.datatype = NS_FLOAT;
    ddef.fillmissingrecords = 1;
    ddef.floatFill = _fillValue;
    ddef.intFill = 0;
    ddef.interval = _sampleTag.getPeriod();

    int ndims = _dimensions.size();
    ddef.dimensions.dimensions_val = 0;
    ddef.dimensions.dimensions_len = ndims;

    if (ndims > 0) {
	ddef.dimensions.dimensions_val = new dimension[ndims];
	for(int i = 0; i < ndims; i++) {
	    ddef.dimensions.dimensions_val[i].name =
	    	(char *)_dimensions[i].getName().c_str();
	    ddef.dimensions.dimensions_val[i].size = _dimensions[i].getValue(0);
	}
    }
    _weightsIndex = -1;
    string weightsName;
    VariableIterator vi = _sampleTag.getVariableIterator();
    for (int i = 0; vi.hasNext(); i++) {
	const Variable* var = vi.next();
	if (var->getType() == Variable::WEIGHT) {
	    _weightsIndex = i;
	    weightsName = var->getName();
	    string::size_type n;
	    while ((n = weightsName.find('.')) != string::npos)
	    	weightsName[n] = '_';
	}
    }

    int nvars = _sampleTag.getVariables().size();
    if (_weightsIndex >= 0) {
	if (_weightsIndex != nvars - 1)
	    throw n_u::IOException(conn->getName(),"connect",
		"weights variable should be last");
	nvars--;
    }

    ddef.fields.fields_val = new field[nvars];
    ddef.fields.fields_len = nvars;
   
    ddef.attrs.attrs_val = new str_attrs[nvars];
    ddef.attrs.attrs_len = nvars;
 
    vi = _sampleTag.getVariableIterator();
    for (int i = 0; vi.hasNext(); ) {
	const Variable* var = vi.next();

	if (var->getType() == Variable::WEIGHT) continue;
	if (ndims > 0) 
	    ddef.fields.fields_val[i].name = (char *)
	    	var->getNameWithoutSite().c_str();
	else
	    ddef.fields.fields_val[i].name = (char *) var->getName().c_str();

	ddef.fields.fields_val[i].units = (char *) var->getUnits().c_str();

	int nattrs = 0;
	if (_weightsIndex >= 0) nattrs++;
	if (var->getLongName().length() > 0) nattrs++;

	if (nattrs > 0) {
	    ddef.attrs.attrs_val[i].attrs.attrs_len = nattrs;
	    ddef.attrs.attrs_val[i].attrs.attrs_val = new str_attr[nattrs];

	    int iattr = 0;
	    if (_weightsIndex >= 0) {
		str_attr *s = ddef.attrs.attrs_val[i].attrs.attrs_val + iattr++;
		s->name = (char *)"counts";
		s->value = (char *)weightsName.c_str();
	    }
	    if (var->getLongName().length() > 0) {
		str_attr *s = ddef.attrs.attrs_val[i].attrs.attrs_val + iattr++;
		s->name = (char *)"long_name";
		s->value = (char *)var->getLongName().c_str();
	    }
	}
	else {
	    ddef.attrs.attrs_val[i].attrs.attrs_len = 0;
	    ddef.attrs.attrs_val[i].attrs.attrs_val = 0;
	}
	i++;
    }

    CLIENT *clnt = conn->getRPCClient();
    enum clnt_stat clnt_stat;
    int ntry;
    int result = 0;
    for (ntry = 0; ntry < 5; ntry++) {
	clnt_stat = clnt_call(clnt, DEFINEDATAREC,
	      (xdrproc_t) xdr_datadef, (caddr_t) &ddef,
	      (xdrproc_t) xdr_int, (caddr_t) &result,
	      conn->getRPCOtherTimeoutVal());
	if (clnt_stat == RPC_SUCCESS) break;
	n_u::Logger::getInstance()->log(LOG_WARNING,
	      "nc_server DEFINEDATAREC failed: %s, timeout=%d secs, ntry=%d",
	      	clnt_sperrno(clnt_stat),conn->getRPCTimeout(),ntry+1);
	if (clnt_stat != RPC_TIMEDOUT && clnt_stat != RPC_CANTRECV) break;
    }
    if (ntry > 0 && clnt_stat == RPC_SUCCESS) 
	n_u::Logger::getInstance()->log(LOG_WARNING,"nc_server OK");

    delete [] ddef.fields.fields_val;
    delete [] ddef.dimensions.dimensions_val;
 
    for (int i=0; i < nvars; i++)
	delete [] ddef.attrs.attrs_val[i].attrs.attrs_val;
    delete [] ddef.attrs.attrs_val;

    if (clnt_stat != RPC_SUCCESS)
	throw n_u::IOException(conn->getName(),"define data rec",
	    clnt_sperrno(clnt_stat));

    // If return is < 0, fetch the error string, and throw exception
    if (result < 0) {
        conn->checkError();
        // checkError should throw an exception if the above call returned a
        // negative result.  If not something's not working right.
	throw n_u::IOException(conn->getName(),"define data rec","unknown error");
    }

    // initialize data record
    _rec.datarecId = result;
    _rec.connectionId = conn->getConnectionId();
   
    _rec.start.start_len = ndims;
    _rec.count.count_len = ndims;
    _rec.start.start_val = new int[ndims];
    _rec.count.count_val = new int[ndims];

    _rec.cnts.cnts_val = 0;
    _rec.cnts.cnts_len = 0;
    if (_weightsIndex >= 0) {
	_rec.cnts.cnts_val = new int[1];
	_rec.cnts.cnts_len = 1;
    }

    _rec.data.data_val = new float[nvars];
    _rec.data.data_len = nvars;
}

void NcVarGroupFloat::write(NetcdfRPCChannel* conn,const Sample* samp,
	int stationIndex) throw(n_u::IOException)
{
    const SampleT<float>* fsamp = static_cast<const SampleT<float>*>(samp);
   
    /* constant members of rec have been initialized */

    /* time in seconds since 1970 Jan 1 00:00 GMT */
    _rec.time = (double)fsamp->getTimeTag() / USECS_PER_SEC;

// #define DEBUG
#ifdef DEBUG
    n_u::UTime ut(fsamp->getTimeTag());
    cerr << ut.format(true,"%Y %m %d %H:%M:%S.%6f ") <<
    	stationIndex << ' ';
    for (unsigned int i = 0; i < fsamp->getDataLength(); i++)
	cerr << fsamp->getConstDataPtr()[i] << ' ';
    cerr << endl;
#endif

    if (stationIndex >= 0) {
	_rec.start.start_val[0] = stationIndex;
	_rec.count.count_val[0] = 1;
    }
    else {
        assert(_rec.start.start_len == 0);
        assert(_rec.count.count_len == 0);
    }

    size_t dlen = fsamp->getDataLength();

    if (_weightsIndex >= 0) {
        if ((signed)fsamp->getDataLength() > _weightsIndex)
	    _rec.cnts.cnts_val[0] =
		(int)rint(fsamp->getConstDataPtr()[_weightsIndex]);
	else _rec.cnts.cnts_val[0] = 0;
	_rec.cnts.cnts_len = 1;
	dlen--;
    }

    const float* fdata = fsamp->getConstDataPtr();
    for (unsigned int i = 0; i < _rec.data.data_len; i++) {
        if (i >= dlen || isnan(fdata[i]))
		_rec.data.data_val[i] = _fillValue;
	else _rec.data.data_val[i] = fdata[i];
    }

    conn->write(&_rec);
}
#endif  // HAS_NC_SERVER_RPC_H
