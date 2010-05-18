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
#include "nidas_qmv/SensorItem.h"

class ConfigWindow;

using namespace std;



class Document {

public:

    Document(ConfigWindow* cw) :
        filename(0),
        _configWindow(cw),
        domdoc(0),
        dummyNum(0)
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


    const xercesc::DOMElement * findSensor(const std::string & sensorIdName);

    void parseFile();
    void printSiteNames();

    unsigned int getNextSensorId();
    unsigned int getNextDSMId();

    // Elements for working with Sensors (add and support functions)
    void addSensor(const std::string & sensorIdName, const std::string & device,
                         const std::string & lcId, const std::string & sfx);
    void addSampAndVar(xercesc::DOMElement *sensorElem, xercesc::DOMNode *dsmNode);

    // Elements for working with DSMs (add and support functions)
    void addDSM(const std::string & dsmName, const std::string & dsmId, 
                const std::string & dsmLocation);
         //      throw (nidas::util::InvalidParameterException, 
         //             InternalProcessingException);
    unsigned int validateDsmInfo(Site *site, const std::string & dsmName, 
                                 const std::string & dsmId);
    xercesc::DOMElement* createDsmOutputElem(xercesc::DOMNode *siteNode);
    xercesc::DOMElement* createDsmServOutElem(xercesc::DOMNode *siteNode);

    // Elements for working with Samples (add and support functions)
    void addSample(const std::string & sampleName, const std::string & sampleId,
                   const std::string & sampleFilter);
    unsigned int validateSampleInfo(DSMSensor *sensor, 
                                    const std::string & sampleId);
    xercesc::DOMElement* createA2DVarElement(xercesc::DOMNode *dsmNode);

    void addA2DVariable(const std::string & a2dVarName, const std::string & a2dVarLongName,
                         const std::string & a2dVarVolts, const std::string & a2dVarChannel,
                         const std::string & a2dVarUnits, vector <std::string> cals);

private:

    std::string *filename;
    const ConfigWindow* _configWindow;
    xercesc::DOMDocument *domdoc;
    int dummyNum;

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
