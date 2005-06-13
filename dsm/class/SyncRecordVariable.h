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
    int getSyncRecOffset() const { return offset; }
    void setSyncRecOffset(int val) { offset = val; }

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
