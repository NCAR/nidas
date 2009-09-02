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


class Document {

public:

    Document() {};
    ~Document() { delete filename; };

    const std::string getFilename() const { return filename; };
    xercesc::DOMNode & getNode() const { return node; };

    void setFilename(std::string f) { if (filename) delete filename; filename = new std::string(f); };
    void setNode(xercesc::DOMNode & n) { node=n; };

private:

    std::string *filename;
    xercesc::DOMNode node;

};

#endif
