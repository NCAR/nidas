/*  
 * DSMTableWidget allows the user to build up a table to display DSM
 * Assumes table will be constructed left to right, top to bottom.
 */
#ifndef dsmtablewidget_h
#define dsmtablewidget_h

#include <iostream>
#include <fstream>

#include <QTableWidget>
#include <QComboBox>

#define NAMECOL 	0
#define NAMEHDR		"Sensor Name"
#define DEVICECOL 	1
#define DEVICEHDR 	"Device"
#define SNCOL 		2
#define SNHDR 		"S/N"
#define IDCOL 		3
#define IDHDR 		"ID"
#define CHANCOL         4
#define CHANHDR         "Channel"
#define SRCOL 		5
#define SRHDR 		"SR"
#define VARCOL 		6
#define VARHDR 		"Variables"
#define GNCOL 		7
#define GNHDR 		"gn"
#define BICOL		8
#define BIHDR		"bi"
#define ADCALCOL	9
#define ADCALHDR	"A/D Cal"
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
        void setID(const QString & id);
        void setSampRate(const float samprate);
        void setOtherVariables(QComboBox *variables);
        void setAnalogVariable(const QString & variable);
        void setAnalogChannel(const int channel);
        void setGain(const int gain);
        void setBiPolar(const int bipolar);
/*
        void setADCal(QTableWidgetItem *adcal);
        //void AddColumnElement(QTableWidget *element);
*/

    private:
        int curRowCount;
        QStringList rowHeaders;


};
#endif
