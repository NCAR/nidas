#include "DSMOtherTable.h"

/* Constructor - create table, set column headers and width */
DSMOtherTable::DSMOtherTable( nidas::core::DSMConfig * dsm,
    xercesc::DOMDocument *doc, QWidget *parent)
       : DSMTableWidget(dsm,doc,parent)
{
    QStringList columnHeaders;

    columnHeaders << columns[NAMEIDX].name << columns[DEVICEIDX].name << columns[SNIDX].name 
                  << columns[CHANIDX].name << columns[SRIDX].name << columns[VARIDX].name
                  << columns[IDIDX].name;
    columns[IDIDX].column=6; // we've left out a few default headers used by analog table

    setColumnCount(columnHeaders.size());
    setHorizontalHeaderLabels(columnHeaders);

    setColumnWidth(columns[SNIDX].column, 40);
    setColumnWidth(columns[CHANIDX].column, 50);
}
