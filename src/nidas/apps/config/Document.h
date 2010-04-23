/*
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $Id$
 ********************************************************************
 * Document.h
 *  a nidas config xml document
 *
 *  Document is the  "controller" in regards to our business model,
 *       containing business logic we don't want in NidasModel/NidasItem
 *       (or just haven't gotten around to moving yet)
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

#include "nidas_qmv/DSMItem.h"

class ConfigWindow;

using namespace std;



class Document {

public:

    Document(ConfigWindow* cw) :
        filename(0),
        _configWindow(cw),
        domdoc(0)
        { }
    ~Document() { delete filename; };

    const char *getDirectory() const;
    const std::string getFilename() const { return *filename; };
    void setFilename(const std::string &f) 
         { if (filename) delete filename; filename = new std::string(f); };

    xercesc::DOMDocument *getDomDocument() const { return domdoc; };
    void setDomDocument(xercesc::DOMDocument *d) { domdoc=d; };
    void writeDocument();

    // get the DSM Node in order to add a sensor to it
    //xercesc::DOMNode *getDSMNode(dsm_sample_id_t dsmId);
    xercesc::DOMNode *getDSMNode(unsigned int dsmId);

        // can't be const because of errorhandler, we'd need to decouple that
    bool writeDOM( xercesc::XMLFormatTarget * const target, 
                   const xercesc::DOMNode * node );

    void addSensor(const std::string & sensorIdName, const std::string & device,
                         const std::string & lcId, const std::string & sfx);

    const xercesc::DOMElement * findSensor(const std::string & sensorIdName);

    void parseFile();
    void printSiteNames();

    unsigned int getNextSensorId();

    void deleteSensor(QModelIndexList selectedIndexList);

    void getSelectedSensorDevices(QModelIndexList & il, std::list <std::string> & devList, std::list<int> & rows);

    void addDSM(const std::string & dsmName, const std::string & dsmId, const std::string & dsmLocation);
         //      throw (nidas::util::InvalidParameterException, InternalProcessingException);

private:

    std::string *filename;
    const ConfigWindow* _configWindow;
    xercesc::DOMDocument *domdoc;

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
