/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-05-01 10:53:29 -0600 (Sun, 01 May 2005) $

    $LastChangedRevision: 1918 $

    $LastChangedBy: maclean $

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/Variable.h $
 ********************************************************************

*/
#ifndef DSM_SYNCRECORDVARIABLE_H
#define DSM_SYNCRECORDVARIABLE_H

#include <Variable.h>

namespace dsm {

/**
 * A Variable associated with a SyncRecord.
 */
class SyncRecordVariable : public Variable
{
public:
    /**
     * Get the index into the sync record of the 
     * first value for this variable.
     */
    int getSyncRecOffset() const { return offset; }

    void setSyncRecOffset(int val) { offset = val; }

    /**
     * Get the index into the sync record of the lag
     * value for this variable.
     * The lag value is the fractional number of micro-seconds
     * to add to the sync record time to get the time tag
     * of the first value of this variable in the sync record.
     * One would use it like so:
     * float lag = sync_rec[var->getLagOffset()];
     * if (!isnan(lag)) tt += lag;
     */
    int getLagOffset() const { return lagOffset; }

    void setLagOffset(int val) { lagOffset = val; }

protected:

    int offset;

    int lagOffset;

private:

    /**
     * Assignment not supported.
     */
    SyncRecordVariable& operator=(const SyncRecordVariable&);

};

}

#endif
