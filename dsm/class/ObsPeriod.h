/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-05-16 12:37:43 -0600 (Mon, 16 May 2005) $

    $LastChangedRevision: 2036 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.atd.ucar.edu/svn/hiaper/ads3/dsm/class/Project.h $
 ********************************************************************

*/

#ifndef DSM_OBSPERIOD_H
#define DSM_OBSPERIOD_H

namespace dsm {

/**
 * An observing period.
 * Right now it just amounts to a name.
 * Eventually things like start and end times could be added.
 */
class ObsPeriod {
public:
    ObsPeriod() {}
    virtual ~ObsPeriod() {}

    /**
     * Set the observing period name.
     * @param val String containing the period name.
     * This class imposes no policy on the name, it
     * can be anything, including empty.
     */
    void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

protected:

    std::string name;

};

}

#endif
