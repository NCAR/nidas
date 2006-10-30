/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-03-03 12:46:08 -0700 (Fri, 03 Mar 2006) $

    $LastChangedRevision: 3299 $

    $LastChangedBy: maclean $

    $HeadURL: http://localhost:5080/svn/nids/branches/ISFF_TREX/dsm/class/NetcdfRPCChannel.cc $
 ********************************************************************

*/

#include <nidas/dynld/isff/NetcdfRPCChannel.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMTime.h>
#include <nidas/core/Site.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NetcdfRPCChannel::NetcdfRPCChannel(): fillValue(1.e37),
	fileLength(SECS_PER_DAY),clnt(0),ntry(0),
        timeInterval(300)
{
    setName("NetcdfRPCChannel");
    setRPCTimeout(300);
    setRPCBatchPeriod(300);
    rpcBatchTimeout.tv_sec = 0;
    rpcBatchTimeout.tv_usec = 0;
}

/* copy constructor */
NetcdfRPCChannel::NetcdfRPCChannel(const NetcdfRPCChannel& x):
	IOChannel(x),
	name(x.name),
	server(x.server),
	fileNameFormat(x.fileNameFormat),
	cdlFileName(x.cdlFileName),
	fillValue(x.fillValue),
	fileLength(x.fileLength),
	clnt(0),
	rpcBatchPeriod(x.rpcBatchPeriod),
	rpcWriteTimeout(x.rpcWriteTimeout),
	rpcOtherTimeout(x.rpcOtherTimeout),
	ntry(0),timeInterval(x.timeInterval)
{
    rpcBatchTimeout.tv_sec = 0;
    rpcBatchTimeout.tv_usec = 0;
}

NetcdfRPCChannel::~NetcdfRPCChannel()
{
}

void NetcdfRPCChannel::addSampleTag(const SampleTag* val)
{
    sampleTags.insert(val);
}

void NetcdfRPCChannel::setName(const std::string& val)
{
    name = val;
}

void NetcdfRPCChannel::setServer(const string& val)
{
    server = val;
    setName(string("ncserver: ") + getServer() + ':' + 
    	getDirectory() + "/" + getFileNameFormat());
}

void NetcdfRPCChannel::setFileNameFormat(const string& val)
{
    fileNameFormat = val;
    setName(string("ncserver: ") + getServer() + ':' + 
    	getDirectory() + "/" + getFileNameFormat());
}

void NetcdfRPCChannel::setDirectory(const string& val)
{
    directory = val;
    setName(string("ncserver: ") + getServer() + ':' + 
    	getDirectory() + "/" + getFileNameFormat());
}

void NetcdfRPCChannel::setRPCTimeout(int secs)
{
    rpcWriteTimeout.tv_sec = secs;
    rpcWriteTimeout.tv_usec = 0;
    rpcOtherTimeout.tv_sec = secs * 5;
    rpcOtherTimeout.tv_usec = 0;
}

int NetcdfRPCChannel::getRPCTimeout() const
{
    return rpcWriteTimeout.tv_sec;
}

struct timeval& NetcdfRPCChannel::getRPCWriteTimeoutVal()
{
    return rpcWriteTimeout;
}

struct timeval& NetcdfRPCChannel::getRPCOtherTimeoutVal()
{
    return rpcOtherTimeout;
}

void NetcdfRPCChannel::setRPCBatchPeriod(int secs)
{
    rpcBatchPeriod = secs;
}

int NetcdfRPCChannel::getRPCBatchPeriod() const
{
    return rpcBatchPeriod;
}

void NetcdfRPCChannel::requestConnection(ConnectionRequester* rqstr)
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

    clnt = clnt_create(getServer().c_str(),
    	NETCDFSERVERPROG, NETCDFSERVERVERS,"tcp");
    if (clnt == (CLIENT *) NULL)
        throw n_u::IOException(getName(),"clnt_create",
	    clnt_spcreateerror(server.c_str()));

    connection conn;
    conn.filenamefmt = (char *)getFileNameFormat().c_str();

    conn.outputdir = (char *)getDirectory().c_str();
    conn.cdlfile = (char *)getCDLFileName().c_str();
    conn.filelength = getFileLength();
    conn.interval = getTimeInterval();

    int result = 0;
    enum clnt_stat clnt_stat;

    if ((clnt_stat = clnt_call(clnt, OPENCONNECTION,
	  (xdrproc_t) xdr_connection, (caddr_t) &conn,
	  (xdrproc_t) xdr_int,  (caddr_t) &result,
	  rpcOtherTimeout)) != RPC_SUCCESS) {
        n_u::IOException e(getName(),"open",
	    clnt_sperror(clnt,server.c_str()));
	clnt_destroy(clnt);
	clnt = 0;
	throw e;
    }

    connectionId = result;
    if (connectionId < 0) {
	clnt_destroy(clnt);
	clnt = 0;
	throw n_u::IOException(getName(),"open",
	  string("perhaps ") + directory + " does not exist on server");
    }

    cerr << "opened: " << getName() << endl;

    lastFlush = time((time_t *)0);

    Project* project = Project::getInstance();

    unsigned int nstations = project->getMaxSiteNumber() -
    	project->getMinSiteNumber() + 1;
    set<int> stns;
    
    set<const SampleTag*>::const_iterator si = getSampleTags().begin();
    for ( ; si != getSampleTags().end(); ++si) {
        const SampleTag* stag = *si;

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
	stationIndexById[stag->getId()] = tagStation - 1;
    }

    vector<ParameterT<int> > dims;

    // all the "non"-station, station 0
    if (stns.size() == 0 && nstations == 1) nstations = 0;
    else {
	if (stns.size() != nstations)
	    n_u::Logger::getInstance()->log(LOG_WARNING,
		"nstations=%d, stns.size()=%d",
		nstations,stns.size());

	ParameterT<int> stnDim;
	stnDim.setName("station");
	stnDim.setValue(nstations);
	dims.push_back(stnDim);
    }
        
    si = getSampleTags().begin();
    for ( ; si != getSampleTags().end(); ++si) {
        const SampleTag* stag = *si;
	
	NcVarGroupFloat* grp = getNcVarGroupFloat(dims,stag);
	if (!grp) {
	    grp = new NcVarGroupFloat(dims,stag,fillValue);
	    grp->connect(this,fillValue);
	    groups.push_back(grp);
	}
#ifdef DEBUG
	cerr << "adding to groupById, tag=" << stag->getId() << endl;
#endif
	groupById[stag->getId()] = grp;
    }
    return this;
}

NcVarGroupFloat* NetcdfRPCChannel::getNcVarGroupFloat(
	const vector<ParameterT<int> >&dims,
	const SampleTag* stag)
{
    list<NcVarGroupFloat*>::const_iterator gi;
    for (gi = groups.begin(); gi != groups.end(); ++gi) {
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
    	groupById.find(sampid);

#ifdef DEBUG
    cerr << "NetcdfRPCChannel::write, sampid=" << 
    	GET_DSM_ID(sampid) << ',' << GET_SHORT_ID(sampid) << 
        " group.size=" << groupById.size() << 
    	" found group=" << (gi != groupById.end()) << endl;
#endif
    if (gi == groupById.end()) return;

    NcVarGroupFloat* g = gi->second;

    int stationIndex = stationIndexById[samp->getId()];
#ifdef DEBUG
    cerr << "NetcdfRPCChannel::write, stationIndex=" << stationIndex <<
    	endl;
#endif

    g->write(this,samp,stationIndex);
}

void NetcdfRPCChannel::write(datarec_float *rec) throw(n_u::IOException)
{

    if (rpcBatchPeriod == 0) {
        nonBatchWrite(rec);
	return;
    }

    /*
     * Every so often check if nc_server actually responds
     */
    if (time(0) - lastFlush > rpcBatchPeriod) return flush();

    /*
     * For RPC batch mode, the timeout is set to 0.
     */
    enum clnt_stat clnt_stat;
    clnt_stat = clnt_call(clnt, WRITEDATARECBATCH_FLOAT,
	(xdrproc_t) xdr_datarec_float, (caddr_t) rec,
	(xdrproc_t) NULL, (caddr_t) NULL,
	rpcBatchTimeout);
    if (clnt_stat != RPC_SUCCESS)
	throw n_u::IOException(getName(),"write",clnt_sperror(clnt,""));
}

void NetcdfRPCChannel::nonBatchWrite(datarec_float *rec) throw(n_u::IOException)
{
    int* result = 0;
    enum clnt_stat clnt_stat;

    for (ntry = 0; ntry < NTRY; ntry++) {
	clnt_stat = clnt_call(clnt, WRITEDATAREC_FLOAT,
	    (xdrproc_t) xdr_datarec_float, (caddr_t) rec,
	    (xdrproc_t) xdr_int, (caddr_t) &result,
	    rpcWriteTimeout);
	if (clnt_stat != RPC_SUCCESS) {
	    bool serious = clnt_stat != RPC_TIMEDOUT && clnt_stat != RPC_CANTRECV;
	    if (serious || ntry > NTRY / 2) {
		n_u::Logger::getInstance()->log(LOG_WARNING,
			"%s: %s, timeout=%d secs, ntry=%d",
		    getName().c_str(),clnt_sperror(clnt,"nc_server not responding"),
		    rpcWriteTimeout.tv_sec ,ntry);
	    }
	    if (serious || ntry >= NTRY) 
		throw n_u::IOException(getName(),"write",
			clnt_sperror(clnt,""));
	}
	else {
	    if (ntry > NTRY / 2)
	    	n_u::Logger::getInstance()->log(LOG_WARNING,"%s: OK",
		    getName().c_str());
	    break;
	}
    }
    // we're ignoring result
}

void NetcdfRPCChannel::flush() throw(n_u::IOException)
{
    enum clnt_stat clnt_stat;
    bool serious = false;

    clnt_stat = clnt_call(clnt,NULLPROC,
	  (xdrproc_t)xdr_void, (caddr_t)NULL,
	  (xdrproc_t)xdr_void, (caddr_t)NULL,
	  rpcWriteTimeout);

    if (clnt_stat != RPC_SUCCESS) {
	serious = (clnt_stat != RPC_TIMEDOUT && clnt_stat != RPC_CANTRECV) ||
	ntry++ >= NTRY;
	n_u::Logger::getInstance()->log(LOG_WARNING,
		"%s: %s, timeout=%d secs, ntry=%d",
	    getName().c_str(),clnt_sperror(clnt,"nc_server not responding"),
	    rpcWriteTimeout.tv_sec ,ntry);
    }
    else {
	if (ntry > 0)
	    n_u::Logger::getInstance()->log(LOG_WARNING,"%s: OK",
		getName().c_str());
	ntry = 0;
	lastFlush = time((time_t*)0);
    }
    if (serious)
	throw n_u::IOException(getName(),"flush",clnt_sperror(clnt,""));
}

void NetcdfRPCChannel::close() throw(n_u::IOException)
{

   if (clnt) {
	int result = 0;
	enum clnt_stat clnt_stat;
	if ((clnt_stat = clnt_call(clnt, CLOSECONNECTION,
	    (xdrproc_t) xdr_int, (caddr_t) &connectionId,
	    (xdrproc_t) xdr_int, (caddr_t) &result,
	    rpcOtherTimeout)) != RPC_SUCCESS) {
	  n_u::IOException e(getName(),"close",clnt_sperror(clnt,""));
	  clnt_destroy(clnt);
	  clnt = 0;
	  throw e;
	}
	clnt_destroy(clnt);
	clnt = 0;
	cerr << "closed: " << getName() << endl;
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
	    if (aname == "server") setServer(aval);
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
	    else if (!aname.compare("floatFill")) {
		istringstream ist(aval);
		float val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),
			aname, aval);
		setFillValue(val);
	    }
	    else if (!aname.compare("timeout")) {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),
			aval, aval);
		setRPCTimeout(val);
	    }
	    else if (!aname.compare("batchPeriod")) {
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

xercesc::DOMElement* NetcdfRPCChannel::toDOMParent(
    xercesc::DOMElement* parent)
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

xercesc::DOMElement* NetcdfRPCChannel::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}

NcVarGroupFloat::NcVarGroupFloat(
	const std::vector<ParameterT<int> >& dims,
        const SampleTag* stag,float fill):
	dimensions(dims),
	sampleTag(*stag),
	weightsIndex(-1),
	fillValue(fill)
{
    rec.start.start_val = 0;
    rec.count.count_val = 0;
    rec.cnts.cnts_val = 0;
    rec.data.data_val = 0;
}

NcVarGroupFloat::~NcVarGroupFloat()
{
    delete [] rec.start.start_val;
    delete [] rec.count.count_val;
    delete [] rec.cnts.cnts_val;
    delete [] rec.data.data_val;
}

void NcVarGroupFloat::connect(NetcdfRPCChannel* conn,float fillValue)
	throw(n_u::IOException)
{
    datadef ddef; 
    ddef.connectionId = conn->getConnectionId(); 
    ddef.rectype = NS_TIMESERIES;
    ddef.datatype = NS_FLOAT;
    ddef.fillmissingrecords = 1;
    ddef.floatFill = fillValue;
    ddef.longFill = 0;
    ddef.interval = sampleTag.getPeriod();

    int ndims = dimensions.size();
    ddef.dimensions.dimensions_val = 0;
    ddef.dimensions.dimensions_len = ndims;

    if (ndims > 0) {
	ddef.dimensions.dimensions_val = new dimension[ndims];
	for(int i = 0; i < ndims; i++) {
	    ddef.dimensions.dimensions_val[i].name =
	    	(char *)dimensions[i].getName().c_str();
	    ddef.dimensions.dimensions_val[i].size = dimensions[i].getValue(0);
	}
    }
    weightsIndex = -1;
    string weightsName;
    VariableIterator vi = sampleTag.getVariableIterator();
    for (int i = 0; vi.hasNext(); i++) {
	const Variable* var = vi.next();
	if (var->getType() == Variable::WEIGHT) {
	    weightsIndex = i;
	    weightsName = var->getName();
	    string::size_type n;
	    while ((n = weightsName.find('.')) != string::npos)
	    	weightsName[n] = '_';
	}
    }

    int nvars = sampleTag.getVariables().size();
    if (weightsIndex >= 0) {
	if (weightsIndex != nvars - 1)
	    throw n_u::IOException(conn->getName(),"connect",
		"weights variable should be last");
	nvars--;
    }

    ddef.fields.fields_val = new field[nvars];
    ddef.fields.fields_len = nvars;
   
    ddef.attrs.attrs_val = new str_attrs[nvars];
    ddef.attrs.attrs_len = nvars;
 
    vi = sampleTag.getVariableIterator();
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
	if (weightsIndex >= 0) nattrs++;
	if (var->getLongName().length() > 0) nattrs++;

	if (nattrs > 0) {
	    ddef.attrs.attrs_val[i].attrs.attrs_len = nattrs;
	    ddef.attrs.attrs_val[i].attrs.attrs_val = new str_attr[nattrs];

	    int iattr = 0;
	    if (weightsIndex >= 0) {
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

    conn->flush();
 
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

    // initialize data record
    rec.datarecId = result;
    rec.connectionId = conn->getConnectionId();
   
    rec.start.start_len = ndims;
    rec.count.count_len = ndims;
    rec.start.start_val = new int[ndims];
    rec.count.count_val = new int[ndims];

    rec.cnts.cnts_val = 0;
    rec.cnts.cnts_len = 0;
    if (weightsIndex >= 0) {
	rec.cnts.cnts_val = new int[1];
	rec.cnts.cnts_len = 1;
    }

    rec.data.data_val = new float[nvars];
    rec.data.data_len = nvars;
}

void NcVarGroupFloat::write(NetcdfRPCChannel* conn,const Sample* samp,
	int stationIndex) throw(n_u::IOException)
{
    const SampleT<float>* fsamp = static_cast<const SampleT<float>*>(samp);
   
    /* constant members of rec have been initialized */

    /* time in seconds since 1970 Jan 1 00:00 GMT */
    rec.time = (double)fsamp->getTimeTag() / USECS_PER_SEC;

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
	rec.start.start_val[0] = stationIndex;
	rec.count.count_val[0] = 1;
    }
    else {
        assert(rec.start.start_len == 0);
        assert(rec.count.count_len == 0);
    }

    size_t dlen = fsamp->getDataLength();

    if (weightsIndex >= 0) {
        if ((signed)fsamp->getDataLength() > weightsIndex)
	    rec.cnts.cnts_val[0] =
		(int)rint(fsamp->getConstDataPtr()[weightsIndex]);
	else rec.cnts.cnts_val[0] = 0;
	rec.cnts.cnts_len = 1;
	dlen--;
    }

    const float* fdata = fsamp->getConstDataPtr();
    for (unsigned int i = 0; i < rec.data.data_len; i++) {
        if (i >= dlen || isnan(fdata[i]))
		rec.data.data_val[i] = fillValue;
	else rec.data.data_val[i] = fdata[i];
    }

    conn->write(&rec);
}
