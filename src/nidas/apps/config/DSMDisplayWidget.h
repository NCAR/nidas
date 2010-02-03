/*  
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
 * DSMDisplayWidget is the top-level QWidget for displaying a DSM
 */
#ifndef _DSMDisplayWidget_h
#define _DSMDisplayWidget_h

#include <QGroupBox>
#include "DSMOtherTable.h"
#include "DSMAnalogTable.h"
#include "DSMTableWidget.h"

#include <nidas/core/DSMConfig.h>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNode.hpp>

#include <list>

class DSMDisplayWidget : public QGroupBox
{
    Q_OBJECT

    public: 
        DSMDisplayWidget( nidas::core::DSMConfig * dsm,
             xercesc::DOMDocument *doc,
             QString & label,
             QWidget *parent = 0
             );

        void setDSMId(const unsigned int dsmid) {
             _dsmId = dsmid; setObjectName(QString::number(dsmid)); dsmAnalogTable->setDSMId(dsmid); 
             dsmOtherTable->setDSMId(dsmid); /* setObjectName() not really needed(?) */ 
        };
        unsigned int getDSMId() {return(_dsmId);};

        xercesc::DOMNode * getDSMNode();
        nidas::core::DSMConfig * getDSMConfig() { return dsmConfig; };

        DSMTableWidget * getAnalogTable() { return dsmAnalogTable; };
        DSMTableWidget * getOtherTable() { return dsmOtherTable; };

        std::list <std::string> getSelectedSensorDevices() const;
 
        void deleteSensors(std::list <std::string>);

    public slots:
        void cvtAct2SS(int row, int column);

    signals:
        void sensorSelected(bool);

    private:
        unsigned int _dsmId;

        nidas::core::DSMConfig * dsmConfig;

        xercesc::DOMDocument *domdoc; // pointer to entire DOM Document
        xercesc::DOMNode *site;       // pointer to the current site node

            // lazily initialized/cached pointer to the <dsm> for this DSM (nidas::core::DSMConfig)
        xercesc::DOMNode *dsmDomNode;

        DSMOtherTable *dsmOtherTable;
        DSMAnalogTable *dsmAnalogTable;

};

#endif
