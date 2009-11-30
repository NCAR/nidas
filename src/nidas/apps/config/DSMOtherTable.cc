#include "DSMOtherTable.h"

/* Constructor - create table, set column headers and width */
DSMOtherTable::DSMOtherTable( nidas::core::DSMConfig * dsm,
    xercesc::DOMDocument *doc, QWidget *parent)
       : DSMTableWidget(dsm,doc,parent)
{
    setColumnCount(NUMCOLS-3);
    QStringList columnHeaders;
    columnHeaders << NAMEHDR << DEVICEHDR << SNHDR 
                  << CHANHDR << SRHDR << VARHDR
                  << IDHDR;

    setHorizontalHeaderLabels(columnHeaders);

    setColumnWidth(SNCOL, 40);
    setColumnWidth(CHANCOL, 50);
}
