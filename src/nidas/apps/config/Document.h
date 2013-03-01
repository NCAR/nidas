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

#include <QStringList>

#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/dom/DOMErrorHandler.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>

#include <nidas/core/Project.h>
#include <nidas/core/SensorCatalog.h>

#include "nidas_qmv/DSMItem.h"
#include "nidas_qmv/SensorItem.h"
#include "nidas_qmv/ProjectItem.h"
#include "nidas_qmv/A2DVariableItem.h"
#include "nidas_qmv/A2DSensorItem.h"
#include "nidas_qmv/PMSSensorItem.h"
#include "nidas_qmv/VariableItem.h"

class ConfigWindow;

using namespace std;
using namespace xercesc;



class Document {

public:

    Document(QString engCalDirRoot, ConfigWindow* cw) :
        filename(0), _configWindow(cw), domdoc(0), 
        _engCalDirExists(false), _isChanged(false), _isChangedBig(false) 
        { _engCalDirRoot = engCalDirRoot; }
    ~Document() { delete filename; };

    const char *getDirectory() const;
    const std::string getFilename() const { return *filename; };
    void setFilename(const std::string &f) 
         { if (filename) delete filename; filename = new std::string(f); };

    xercesc::DOMDocument *getDomDocument() const { return domdoc; };
    void setDomDocument(xercesc::DOMDocument *d) { domdoc=d; };
    bool writeDocument();

    string getProjectName() const ;
    void setProjectName(string projectName);

    // get the DSM Node in order to add a sensor to it
    //xercesc::DOMNode *getDSMNode(dsm_sample_id_t dsmId);
    xercesc::DOMNode *getDSMNode(unsigned int dsmId);

        // can't be const because of errorhandler, we'd need to decouple that
    bool writeDOM( xercesc::XMLFormatTarget * const target, 
                   const xercesc::DOMNode * node );


    const xercesc::DOMElement * findSensor(const std::string & sensorIdName);

    void parseFile();
    void printSiteNames();
    vector <std::string> getSiteNames();

    unsigned int getNextSensorId();
    unsigned int getNextDSMId();
    list <int> getAvailableA2DChannels();

    // Elements for working with Sensors (add and support functions)
    void addSensor(const std::string & sensorIdName, 
                   const std::string & device,
                   const std::string & lcId, 
                   const std::string & sfx, 
                   const std::string & a2dTempSfx, 
                   const std::string & a2dSNFname,
                   const std::string & pmsSN,
                   const std::string & resltn);
    void updateSensor(const std::string & sensorIdName, 
                      const std::string & device, 
                      const std::string & lcId,
                      const std::string & sfx, 
                      const std::string & a2dTempSfx,
                      const std::string & a2dSNFname,
                      const std::string & pmsSN,
                      const std::string & resltn,
                      QModelIndexList indexList);
    void addA2DRate(xercesc::DOMElement *sensorElem,
                    xercesc::DOMNode *dsmNode,
                    const std::string & a2dSNFname);
    void addA2DCalFile(xercesc::DOMElement *sensorElem,
                    xercesc::DOMNode *dsmNode,
                    const std::string & a2dSNFname);
    void addSampAndVar(xercesc::DOMElement *sensorElem, 
                       xercesc::DOMNode *dsmNode, 
                       const std::string & a2dTempSfx);

    void updateSampAndVar(xercesc::DOMElement *sensorElem, 
                          xercesc::DOMNode *dsmNode, 
                          const std::string & a2dTempSfx);

    void updateSensorDOM(SensorItem * sItem, const std::string & device,
                               const std::string & lcId, const std::string & sfx);

    // Elements for working with DSMs (add and support functions)
    void addDSM(const std::string & dsmName, const std::string & dsmId, 
                const std::string & dsmLocation);
         //      throw (nidas::util::InvalidParameterException, 
         //             InternalProcessingException);
    void updateDSM(const std::string & dsmName,
                   const std::string & dsmId,
                   const std::string & dsmLocation,
                   QModelIndexList indexList);
    void updateDSMDOM(DSMItem* dsmItem, const std::string & dsmName,
                      const std::string & dsmId, 
                      const std::string & dsmLocation);
    unsigned int validateDsmInfo(Site *site, const std::string & dsmName, 
                                 const std::string & dsmId);
    xercesc::DOMElement* createDsmOutputElem(xercesc::DOMNode *siteNode);
    xercesc::DOMElement* createDsmServOutElem(xercesc::DOMNode *siteNode);

    unsigned int validateSampleInfo(DSMSensor *sensor, 
                                    const std::string & sampleId);
    xercesc::DOMElement* createA2DTempVarElement(xercesc::DOMNode *seniorNode, 
                                                 const std::string & a2dTempSfx);
    xercesc::DOMElement* createA2DTempParmElement(xercesc::DOMNode *seniorNode);
    xercesc::DOMElement* createSampleElement(xercesc::DOMNode *sensorNode, 
                         const std::string & sampleId, 
                         const std::string & sampleRate,
                         const std::string & sampleFilter);

    void addA2DVariable(const std::string & a2dVarNamePfx, 
                        const std::string & a2dVarNameSfx,
                        const std::string & a2dVarLongName,
                        const std::string & a2dVarVolts,
                        const std::string & a2dVarChannel,
                        const std::string & a2dSR, 
                        const std::string & a2dVarUnits, 
                        vector <std::string> cals);

    void insertA2DVariable(NidasModel            *model,
                           A2DSensorItem         *sensorItem,
                           DOMNode               *sensorNode,
                           DSMAnalogSensor       *analogSensor,
                           const std::string     &a2dVarNamePfx,
                           const std::string     &a2dVarNameSfx,
                           const std::string     &a2dVarLongName,
                           const std::string     &a2dVarVolts,
                           const std::string     &a2dVarChannel,
                           const std::string     &a2dVarSR,
                           const std::string     &a2dVarUnits,
                           vector <std::string>  cals);

    void updateVariable(VariableItem * varItem,
                        const std::string & VarName, 
                        const std::string & VarLongName,
                        const std::string & SR, 
                        const std::string & VarUnits, 
                        vector <std::string> cals,
                        bool useCalfile);

    void addCalibElem(vector <std::string> cals, 
                      const std::string & a2dVarUnits, 
                      xercesc::DOMNode *sampleNode, 
                      xercesc::DOMElement *varElem);

    void addVarCalFileElem(std::string varCalFileName,
                              const std::string & varUnits, 
                              const std::string & siteName,
                              xercesc::DOMNode *sampleNode,
                              xercesc::DOMElement *varElem);

    void addPMSParms(xercesc::DOMElement *sensorElem,
                        xercesc::DOMNode *dsmNode,
                        const std::string & pmsSN,
                        const std::string & pmsResltn);

    // For informing user about saving the configuration
    // changed means save in place, changedBig means that they should
    // consider a new filename (esp if done mid-project) and 
    // missingEngCalFiles are for when a variable is missing cal info
    bool isChanged() {return _isChanged;}
    void setIsChanged(bool changed) {_isChanged=changed;}
    bool isChangedBig() {return _isChangedBig;}
    void setIsChangedBig(bool changed) {_isChangedBig=changed;}
    vector <QString> getMissingEngCalFiles() {return _missingEngCalFiles;}
    void addMissingEngCalFile(QString filename);
    bool engCalDirExists() { return _engCalDirExists; }
    QString getEngCalDir() { return _engCalDir; }

private:

    Project* _project;
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


// If we had a vector of A2DVariable structures:
    struct A2DVariableInfo {
        std::string a2dVarNamePfx;
        std::string a2dVarNameSfx;
        std::string a2dVarLongName;
        std::string a2dVarVolts;
        std::string a2dVarChannel;
        std::string a2dVarSR;
        std::string a2dVarUnits;
        vector <std::string> cals;
    };

    bool isNum(std::string str);

    QString _engCalDir;
    QString _engCalDirRoot;
    std::vector <QString> _engCalFiles;
    bool _engCalDirExists;
    vector <QString> _missingEngCalFiles;
    bool _isChanged;
    bool _isChangedBig;
};

#endif
