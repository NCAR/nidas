
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_TESTSAMPLECLIENT_H
#define DSM_TESTSAMPLECLIENT_H

#include <SampleClient.h>

namespace dsm {

/**
 * A little SampleClient for testing purposes.  Currently
 * just prints out some fields from the Samples it receives.
 */
class TestSampleClient : public SampleClient {
public:

  bool receive(const Sample *s) throw();

};

}

#endif
