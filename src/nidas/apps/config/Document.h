/*
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $Id$
 ********************************************************************
 * Document.h
 *  a nidas config xml document
 */

#ifndef _Document_h
#define _Document_h

#include <libgen.h>


class Document {

public:

    Document() { filename = 0; };
    ~Document() { delete filename; };

    const std::string getFilename() const { return *filename; };
    xercesc::DOMDocument *getDomDocument() const { return domdoc; };

    const char *getDirectory() {
        static char buf[MAXPATHLEN];
        strncpy(buf,filename->c_str(),MAXPATHLEN-1);
        return dirname(buf);
        }

    void setFilename(const std::string &f) { if (filename) delete filename; filename = new std::string(f); };
    void setDomDocument(xercesc::DOMDocument *d) { domdoc=d; };

private:

    std::string *filename;
    xercesc::DOMDocument *domdoc;

};

#endif
