/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <SampleInputHeader.h>

#include <Project.h>

using namespace dsm;
using namespace std;

/* static */
struct SampleInputHeader::headerField SampleInputHeader::headers[] = {
    { "archive version:", &SampleInputHeader::setArchiveVersion,
	    &SampleInputHeader::getArchiveVersion },
    { "software version:", &SampleInputHeader::setSoftwareVersion,
	    &SampleInputHeader::getSoftwareVersion },
    { "project name:", &SampleInputHeader::setProjectName,
	    &SampleInputHeader::getProjectName },
    { "site name:", &SampleInputHeader::setSiteName,
	    &SampleInputHeader::getSiteName },
    { "observation period name:", &SampleInputHeader::setObsPeriodName,
	    &SampleInputHeader::getObsPeriodName },
    { "xml name:", &SampleInputHeader::setXMLName,
	    &SampleInputHeader::getXMLName },
    { "xml version:", &SampleInputHeader::setXMLVersion,
	    &SampleInputHeader::getXMLVersion },
    { "end header\n", 0, 0 },
};

void SampleInputHeader::check(IOStream* iostream)
	throw(atdUtil::IOException)
{
    char buf[256];

    if (iostream->read(buf,10) != 10 || strncmp(buf,"NCAR ADS3\n",10))
    	throw atdUtil::IOException(iostream->getName(),"open",
		"is not an \"NCAR ADS3\" file");

    int ntags = sizeof(headers)/sizeof(struct headerField);

    unsigned int ic = 0;
    for ( ;; ) {
        if (iostream->read(buf+ic,1) == 0) break;
	ic++;
	int itag;
	for (itag = 0; itag < ntags; itag++)
	    if (!strncmp(headers[itag].tag,buf,ic)) break;

	// no match to any tag, assume it is start of data
	if (itag == ntags) break;

	if (ic == strlen(headers[itag].tag)) {	// complete tag match
	    ic = 0;
	    if (!headers[itag].setFunc) break;		// endheader tag
	    for(;;) {
		if (iostream->read(buf+ic,1) == 0) return; // EOF
		if (buf[ic] == '\n') break;
		if (++ic == sizeof(buf)) break;
	    }
	    // no newline found for this tag value
	    if (ic == sizeof(buf) || buf[ic] != '\n') 
		throw atdUtil::IOException(iostream->getName(),"open",
		    string("no value found for tag ") + headers[itag].tag);

	    unsigned int is = 0;
	    for (is = 0; is < ic && buf[is] == ' '; is++);
	    string value = string(buf+is,ic-is);

	    // cerr << headers[itag].tag << ' ' << value << endl;
	    (this->*headers[itag].setFunc)(value);

	    if (headers[itag].getFunc == &SampleInputHeader::getObsPeriodName &&
		    value.compare("unknown")) {
		ObsPeriod obs(value);
		Project::getInstance()->setCurrentObsPeriod(obs);
	    }
	        
	    ic = 0;
	}
    } 
    iostream->putback(buf,ic);
}

void SampleInputHeader::write(IOStream* iostream)
	throw(atdUtil::IOException)
{
    const char* str = "NCAR ADS3\n";
    iostream->write(str,strlen(str));

    int ntags = sizeof(headers)/sizeof(struct headerField);

    int itag;
    for (itag = 0; itag < ntags; itag++) {
	str = headers[itag].tag;
	iostream->write(str,strlen(str));
	if (headers[itag].getFunc) {
	    str = " ";
	    iostream->write(str,strlen(str));
	    str = (this->*headers[itag].getFunc)().c_str();
	    iostream->write(str,strlen(str));
	    str = "\n";
	    iostream->write(str,strlen(str));
	}
    } 
}

