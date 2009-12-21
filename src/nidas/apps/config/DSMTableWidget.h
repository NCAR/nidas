/*  
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
 * DSMTableWidget allows the user to build up a table to display DSM
 * Assumes table will be constructed left to right, top to bottom.
 */
#ifndef dsmtablewidget_h
#define dsmtablewidget_h

#include <iostream>
#include <fstream>

#include <vector>
#include <list>

#include <QTableWidget>
#include <QComboBox>

#include <nidas/core/DSMConfig.h>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNode.hpp>

#define NAMEIDX 	0
#define NAMEHDR		"Sensor Name"
#define DEVICEIDX 	1
#define DEVICEHDR 	"Device"
#define SNIDX 		2
#define SNHDR 		"S/N"
#define CHANIDX         3
#define CHANHDR         "Chan"
#define SRIDX 		4
#define SRHDR 		"SR"
#define VARIDX 		5
#define VARHDR 		"Variables"
#define GNIDX 		6
#define GNHDR 		"gn"
#define BIIDX		7
#define BIHDR		"bi"
#define ADCALIDX	8
#define ADCALHDR	"A/D Cal"
#define IDIDX 		9
#define IDHDR 		"ID"

class DSMTableWidget : public QTableWidget
{
    Q_OBJECT

    public: 
        DSMTableWidget( nidas::core::DSMConfig * dsm,
             xercesc::DOMDocument *doc,
             QWidget *parent = 0
             );

        void addRow();
        void setName(const std::string & name);
        void setDevice(const std::string & device);
        void setSerialNumber(const std::string & name);
        void setDSMId(const unsigned int dsmid) {_dsmId = dsmid;};
        unsigned int getDSMId() {return(_dsmId);};
        void setNidasId(const unsigned int & sensor_id);
        void setSampRate(const float samprate);
        void setOtherVariables(QComboBox *variables);
        void setAnalogVariable(const QString & variable);
        void setAnalogChannel(const int channel);
        void setGain(const int gain);
        void setBiPolar(const int bipolar);
        void setA2DCal(const QString & a2dcal);
        //void AddColumnElement(QTableWidget *element);

        xercesc::DOMNode * getDSMNode();
        nidas::core::DSMConfig * getDSMConfig() { return dsmConfig; };

        void appendSelectedSensorDevices( std::list <std::string> & ) const;

        void deleteSensors( std::list <std::string> & );
        void removeDevice(int row);

    protected:

        class _ColumnHeader { // headers for the QTableWidget's columns
            public:
              int column;   // column index (0-based)
              const char *name; // column name string (becomes a QString)
         };

         static _ColumnHeader _theHeaders[]; // array of possible column headers in their default order

         std::vector<_ColumnHeader> columns;    // column headers for this


    private:
        QStringList rowHeaders;
        unsigned int _dsmId;
        //std::vector<unsigned int> sensorIDs;

        nidas::core::DSMConfig * dsmConfig;

        xercesc::DOMDocument *domdoc; // pointer to entire DOM Document

            // lazily initialized/cached pointer to the <dsm> for this DSM (nidas::core::DSMConfig)
        xercesc::DOMNode *dsmDomNode;


};
#endif
