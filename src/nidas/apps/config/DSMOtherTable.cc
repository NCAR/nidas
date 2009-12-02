#include "DSMOtherTable.h"

/* Constructor - create table, set column headers and width */
DSMOtherTable::DSMOtherTable( nidas::core::DSMConfig * dsm,
    xercesc::DOMDocument *doc, QWidget *parent)
       : DSMTableWidget(dsm,doc,parent)
{
    setColumnCount(NUMIDXS-3);
    QStringList columnHeaders;
    columnHeaders << NAMEHDR << DEVICEHDR << SNHDR 
                  << CHANHDR << SRHDR << VARHDR
                  << IDHDR;

    setHorizontalHeaderLabels(columnHeaders);

    setColumnWidth(SNIDX, 40);
    setColumnWidth(CHANIDX, 50);
}
