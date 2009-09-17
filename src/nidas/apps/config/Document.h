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

#include <iostream>
#include <fstream>
#include <string>

#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/dom/DOMErrorHandler.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>

#include <nidas/core/Project.h>


using namespace std;



class Document {

public:

    Document() { filename = 0; };
    ~Document() { delete filename; };

    const char *getDirectory() const;
    const std::string getFilename() const { return *filename; };
    void setFilename(const std::string &f) { if (filename) delete filename; filename = new std::string(f); };

    xercesc::DOMDocument *getDomDocument() const { return domdoc; };
    void setDomDocument(xercesc::DOMDocument *d) { domdoc=d; };
    void writeDocument();

    nidas::core::Project *getProject() const { return project; };
    void setProject(nidas::core::Project *p) { project=p; };

        // can't be const because of errorhandler, we'd need to decouple that
    bool writeDOM( xercesc::XMLFormatTarget * const target, const xercesc::DOMNode * node );

private:

    std::string *filename;
    xercesc::DOMDocument *domdoc;
    nidas::core::Project *project;

    // stoopid error handler for development/testing
    // can't be inner class so writeDOM can be const
    class MyErrorHandler : public xercesc::DOMErrorHandler {
    public:
        bool handleError(const xercesc::DOMError & domError) {
            cerr << domError.getMessage() << endl;
            return true;
        }
    } errorHandler;

};

#endif
