/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision: 671 $

    $LastChangedBy: maclean $

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/
#ifndef DSMSENSOR_H
#define DSMSENSOR_H

#include <atdUtil/IOException.h>
#include <SampleSource.h>
#include <DOMable.h>
#include <SampleParseException.h>

#include <dsm_sample.h>

#include <string>
#include <fcntl.h>

/**
 * An interface for a DSM Sensor.
 */
class DSMSensor : public dsm::SampleSource, public dsm::DOMable {

public:

    /**
     * Create a sensor.
     */
    DSMSensor();

    /**
     * Create a sensor, giving its device name.  No IO (open/read/write/ioctl)
     * operations to the sensor are performed in the constructor.
     */
    DSMSensor(const std::string& n);

    virtual ~DSMSensor() {}

    void setDeviceName(const std::string& val) { devname = val; }

    const std::string& getDeviceName() const { return devname; }

    virtual int getReadFd() const = 0;

    /**
     * Retrieve this sensor's id number.
     */
    virtual int getId() const { return id; };

    /**
     * Set a unique identification number on this sensor.
     * The samples from this sensor will contain this id.
     */
    virtual void setId(int val) { id = val; };


    /**
    * Open the device. flags are a combination of O_RDONLY, O_WRONLY.
    */
    virtual void open(int flags) throw(atdUtil::IOException) = 0;

    /**
    * Read from the device (duh). Behaves like read(2) system call,
    * without a file descriptor argument, and with an IOException.
    */
    virtual ssize_t read(void *buf, size_t len) throw(atdUtil::IOException) = 0;	

    /**
    * Write to the device (duh). Behaves like write(2) system call,
    * without a file descriptor argument, and with an IOException.
    */
    virtual ssize_t write(const void *buf, size_t len) throw(atdUtil::IOException) = 0;

    /**
    * Perform an ioctl on the device. request is an integer
    * value which must be supported by the device. Normally
    * this is a value from a header file for the device.
    */
    virtual void ioctl(int request, void* buf, size_t len) throw(atdUtil::IOException) = 0;

    /**
     * Overloaded ioctl method, used when doing an ioctl set from
     * a pointer to constant user data.
     */
    virtual void ioctl(int request, const void* buf, size_t len) throw(atdUtil::IOException) = 0;

    /**
    * close
    */
    virtual void close() throw(atdUtil::IOException) = 0;

    /**
    * When the PortSelector select() system call has determined
    * that there is data available to read on this sensor,
    * PortSelector calls this readSamples method().
    * readSmples reads the raw data samples the port and calls
    * the process() method after reading each sample.  The process()
    * method can do any further processing of the sample before doing a
    * distribute() to the SampleClients.
    */
    virtual dsm_sample_time_t readSamples()
    	throw(dsm::SampleParseException,atdUtil::IOException) = 0;

    void initStatistics();
    void calcStatistics(unsigned long periodMsec);
    float getObservedSamplingRate() const;
    float getReadRate() const;

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);


protected:

    std::string devname;

    int id;

    /**
     * DSMSensor maintains some counters that can be queried
     * to provide the current status.
     */
    time_t initialTimeSecs;
    int minSampleLength[2];
    int maxSampleLength[2];
    int readErrorCount[2];	// [0] is recent, [1] is cumulative
    int writeErrorCount[2];	// [0] is recent, [1] is cumulative
    int currStatsIndex;
    int reportStatsIndex;
    int nsamples;

    /**
    * Observed number of samples per second.
    */
    float sampleRateObs;

};

#endif
