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

#include <QTableWidget>
#include <QComboBox>

#define NAMECOL 	0
#define NAMEHDR		"Sensor Name"
#define DEVICECOL 	1
#define DEVICEHDR 	"Device"
#define SNCOL 		2
#define SNHDR 		"S/N"
#define CHANCOL         3
#define CHANHDR         "Chan"
#define SRCOL 		4
#define SRHDR 		"SR"
#define VARCOL 		5
#define VARHDR 		"Variables"
#define GNCOL 		6
#define GNHDR 		"gn"
#define BICOL		7
#define BIHDR		"bi"
#define ADCALCOL	8
#define ADCALHDR	"A/D Cal"
#define IDCOL 		9
#define IDHDR 		"ID"
#define NUMCOLS 	10

class DSMTableWidget : public QTableWidget
{
    Q_OBJECT

    public: 
        DSMTableWidget(QWidget *parent = 0);

        void addRow();
        void setName(const std::string & name);
        void setDevice(const std::string & device);
        void setSerialNumber(const std::string & name);
        void setDSMID(const unsigned int dsmid) {_dsmID = dsmid;};
        unsigned int getDSMID() {return(_dsmID);};
        void setID(const unsigned int & sensor_id);
        void setSampRate(const float samprate);
        void setOtherVariables(QComboBox *variables);
        void setAnalogVariable(const QString & variable);
        void setAnalogChannel(const int channel);
        void setGain(const int gain);
        void setBiPolar(const int bipolar);
        void setA2DCal(const QString & a2dcal);
        //void AddColumnElement(QTableWidget *element);

    private:
        int curRowCount;
        QStringList rowHeaders;
        unsigned int _dsmID;
        std::vector<unsigned int> sensorIDs;


};
#endif
