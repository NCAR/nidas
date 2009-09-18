#include <QtGui>
#include "SensorCatalogWidget.h"

#include <iostream>
#include <fstream>


/* Constructor - create table, set column headers and width */
SensorCatalogWidget::SensorCatalogWidget(QWidget *parent)
       : QTableWidget(parent)
{
    curRowCount = 0;

    setColumnCount(NUMSCOLS); 
    QStringList columnHeaders;
    columnHeaders << NAMEHDR << VARHDR; 

    setHorizontalHeaderLabels(columnHeaders);
    //resizeColumnsToContents();
    //setAlternatingRowColors(true);
    setAutoFillBackground(true);

}

void SensorCatalogWidget::addRow()
{
    curRowCount++;
    setRowCount(curRowCount);    
    if (curRowCount > 4) setBackgroundRole(QPalette::AlternateBase);
    rowHeaders << "";
    setVerticalHeaderLabels(rowHeaders);
}

void SensorCatalogWidget::setName(const std::string & name)
{
    QString tmpStr;

    tmpStr.append(QString::fromStdString(name));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    //setForegroundRole(QPalette::AlternateBase);
    setItem(curRowCount-1, NAMESCOL, tempWidgetItem);
    resizeColumnToContents(NAMESCOL);
}

void SensorCatalogWidget::setOtherVariables(QComboBox *variables)
{
    setCellWidget(curRowCount-1, VARSCOL, variables);
    setColumnWidth(VARSCOL, 100);
}

