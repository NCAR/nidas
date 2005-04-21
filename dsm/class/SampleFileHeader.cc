/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <SampleFileHeader.h>

using namespace dsm;
using namespace std;

static struct headerField {
    /* a tag in the file header */
    const char* tag;
    /* ptr to setXXX member function for setting an attribute of this
     * class, based on the value of the tag from the IOStream.
     */
    void (SampleFileHeader::* setFunc)(const string&);
    /* ptr to getXXX member function for getting an attribute of this
     * class, in order to write the value of the tag to the IOStream.
     */
    const string& (SampleFileHeader::* getFunc)() const;
} headers[] = {
    { "archive version:", &SampleFileHeader::setArchiveVersion,
	    &SampleFileHeader::getArchiveVersion },
    { "software version:", &SampleFileHeader::setSoftwareVersion,
	    &SampleFileHeader::getSoftwareVersion },
    { "project:", &SampleFileHeader::setProjectName,
	    &SampleFileHeader::getProjectName },
    { "xml name:", &SampleFileHeader::setXMLName,
	    &SampleFileHeader::getXMLName },
    { "xml version:", &SampleFileHeader::setXMLVersion,
	    &SampleFileHeader::getXMLVersion },
};

void SampleFileHeader::check(IOStream* iostream)
	throw(atdUtil::IOException)
{
    size_t l;
    char buf[256];

    if (iostream->read(buf,10) != 10 ||strncmp(buf,"NCAR ADS3\n",10))
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

	if (itag == ntags) break;	// no match to any tag

	if (ic == strlen(headers[itag].tag)) {	// complete tag match
	    ic = 0;
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

	    cerr << headers[itag].tag << " = " << value << endl;
	    (this->*headers[itag].setFunc)(value);
	    ic = 0;
	}
    } 
    cerr << "iostream->putback, ic=" << ic << endl;
    iostream->putback(buf,ic);
    cerr << "iostream->available=" << iostream->available() << endl;
}

void SampleFileHeader::write(IOStream* iostream)
	throw(atdUtil::IOException)
{
    const char* str = "NCAR ADS3\n";
    iostream->write(str,strlen(str));

    int ntags = sizeof(headers)/sizeof(struct headerField);

    int itag;
    for (itag = 0; itag < ntags; itag++) {
	str = headers[itag].tag;
	iostream->write(str,strlen(str));
	str = " ";
	iostream->write(str,strlen(str));
	str = (this->*headers[itag].getFunc)().c_str();
	iostream->write(str,strlen(str));
	str = "\n";
	iostream->write(str,strlen(str));
    } 
}

