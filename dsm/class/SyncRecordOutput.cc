/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#include <SyncRecordOutput.h>
#include <DSMSerialSensor.h>
#include <DSMArincSensor.h>
#include <Aircraft.h>
#include <irigclock.h>

#include <math.h>

using namespace dsm;
using namespace std;

CREATOR_ENTRY_POINT(SyncRecordOutput)

SyncRecordOutput::SyncRecordOutput():
	sorter(250)
{
    sorter.addSampleClient(&generator);
    generator.addSampleClient(this);
}

SyncRecordOutput::~SyncRecordOutput()
{
}

void SyncRecordOutput::init() throw(atdUtil::IOException)
{
    try {
	sorter.start();
    }
    catch(const atdUtil::Exception& e) {
        throw atdUtil::IOException("SyncRecordOutput","init",e.what());
    }
    SampleOutputStream::init();
}

void SyncRecordOutput::flush() throw(atdUtil::IOException)
{
    sorter.flush();
    SampleOutputStream::flush();
}

void SyncRecordOutput::close() throw(atdUtil::IOException)
{
    sorter.interrupt();
    try {
	sorter.join();
    }
    catch (const atdUtil::Exception& e) {
        throw atdUtil::IOException("SyncRecordOutput","close",e.what());
    }
    SampleOutputStream::close();
}

void SyncRecordOutput::setDSMConfig(const DSMConfig* dsm)
{
    const Aircraft* aircraft = dsm->getAircraft();
    generator.setAircraft(aircraft);
}

bool SyncRecordOutput::receive(const Sample* samp)
        throw(SampleParseException, atdUtil::IOException)
{
    cerr << "doing sorter.receive" << endl;
    return sorter.receive(samp);
}
