/*  
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
 * DSMOtherTable builds a DSMTableWidget for "other" (non-analog) sensors
 */
#ifndef _DSMOtherTable_h
#define _DSMOtherTable_h

#include "DSMTableWidget.h"

class DSMOtherTable : public DSMTableWidget
{
    public: 
        DSMOtherTable( nidas::core::DSMConfig * dsm,
             xercesc::DOMDocument *doc,
             QWidget *parent = 0
             ) : DSMTableWidget(dsm,doc,parent) {};

};

#endif
