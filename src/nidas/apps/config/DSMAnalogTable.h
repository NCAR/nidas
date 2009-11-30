/*  
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
 * DSMAnalogTable builds a DSMTableWidget for Analog cards/sensors
 */
#ifndef _DSMAnalogTable_h
#define _DSMAnalogTable_h

#include "DSMTableWidget.h"

class DSMAnalogTable : public DSMTableWidget
{
    public: 
        DSMAnalogTable( nidas::core::DSMConfig * dsm,
             xercesc::DOMDocument *doc,
             QWidget *parent = 0
             ) : DSMTableWidget(dsm,doc,parent) {};

};

#endif
