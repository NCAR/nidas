/*  
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
 * SensorCatalogWidget shows the user all sensors that are found in
 * the project's sensor catalog.
 *
 * Assumes table will be constructed left to right, top to bottom.
 */
#ifndef sensorcatalogwidget_h
#define sensorcatalogwidget_h

#include <iostream>
#include <fstream>

#include <QTableWidget>
#include <QComboBox>

#define NAMESCOL 	0
#define NAMEHDR		"Sensor Name"
#define VARSCOL 	1
#define VARHDR 		"Variables"
#define NUMSCOLS 	2

class SensorCatalogWidget : public QTableWidget
{
    Q_OBJECT

    public: 
        SensorCatalogWidget(QWidget *parent = 0);

        void addRow();
        void setName(const std::string & name);
        void setOtherVariables(QComboBox *variables);
        //void AddColumnElement(QTableWidget *element);

    private:
        int curRowCount;
        QStringList rowHeaders;


};
#endif
