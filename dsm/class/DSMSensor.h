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
#include <SampleClient.h>
#include <SampleSource.h>
#include <RawSampleSource.h>
#include <Variable.h>
#include <DOMable.h>
#include <SampleParseException.h>

#include <dsm_sample.h>

#include <string>
#include <list>

#include <fcntl.h>

namespace dsm {
/**
 * DSMSensor provides the basic support for reading, processing
 * and distributing samples from a sensor attached to a DSM.
 *
 * DSMSensor has a no-arg constructor, and can fill in its attributes
 * from an XML DOM element with fromDOMElement().
 * One attribute of a DSMSensor is the system device
 * name associated with this sensor, e.g. "/dev/xxx0".
 * Once a device name has been set, then a user of this sensor
 * can call open(),  and then ioctl(), read() and write().
 * These methods must be implemented by a derived class,
 * RTL_DSMSensor, for example.
 *
 * SampleClient's can call
 * addRawSampleClient()/removeRawSampleClient() if they want to
 * receive raw SampleT's from this sensor.
 *
 * SampleClient's can also call
 * addSampleClient()/removeSampleClient() if they want to
 * receive (minimally) processed SampleT's from this sensor.
 *
 * A common usage of a DSMSensor is to add it to a PortSelector
 * object with PortSelector::addSensorPort().
 * When the PortSelector::run method has determined that there is data
 * available on a DSMSensor's file descriptor, it will then call
 * the readSamples() method which reads the samples from the
 * file descriptor, processes them, and forwards the raw and processed
 * samples to all associated SampleClient's of this DSMSensor.
 *
 */
class DSMSensor : public RawSampleSource, public SampleSource,
	public SampleClient, public DOMable {

public:

    /**
     * Create a sensor.
     */
    DSMSensor();

    /**
     * /deprecated Construct a sensor, and provide the device name.
     * Use the no-arg constructor instead and use the setDeviceName() method.
     */
    DSMSensor(const std::string& n);

    virtual ~DSMSensor();

    virtual void setDeviceName(const std::string& val) { devname = val; }

    const std::string& getDeviceName() const { return devname; }

    virtual void addVariable(Variable* var);

    const std::list<const Variable*>& getVariables() const { return constVariables; }

    virtual int getReadFd() const = 0;

    virtual bool isClock() const { return false; }

    /**
     * Retrieve this sensor's id number.
     */
    int getId() const { return id; };

    /**
     * Set a unique identification number on this sensor.
     * The samples from this sensor will contain this id.
     */
    void setId(int val) { id = val; };

    /**
    * Open the device. flags are a combination of O_RDONLY, O_WRONLY.
    */
    virtual void open(int flags) throw(atdUtil::IOException) = 0;

    /**
     * How do I want to be opened.  The user can ignore it if they want to.
     */
    virtual int getDefaultMode() const { return O_RDONLY; }

    /**
    * Read from the device (duh). Behaves like read(2) system call,
    * without a file descriptor argument, and with an IOException.
    */
    virtual size_t read(void *buf, size_t len) throw(atdUtil::IOException) = 0;	

    /**
    * Write to the device (duh). Behaves like write(2) system call,
    * without a file descriptor argument, and with an IOException.
    */
    virtual size_t write(const void *buf, size_t len) throw(atdUtil::IOException) = 0;

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
    * Read samples from my associated file descriptor,
    * return them in a list.
    * process them, and pass them onto my SampleClient's.
    *
    * readSamples() assumes that the data read from
    * the file descriptor is formatted into samples
    * in the format of a struct dsm_sample, i.e. a
    * 4 byte unsigned integer time-tag (milliseconds since
    * midnight GMT), followed by a 4 byte unsigned integer data
    * length, and then length number of bytes of data.
    */
    dsm_sample_time_t readSamples()
    	throw(SampleParseException,atdUtil::IOException);

    bool receive(const Sample *s)
  	throw(SampleParseException, atdUtil::IOException);
    /**
     * Apply further necessary processing to a raw sample
     * from this DSMSensor. Return the resultant sample(s)
     * in result.  The default implementation
     * of process() simply puts the Sample into result.
     */
    virtual bool process(const Sample*,std::list<const Sample*>& result)
    	throw(SampleParseException,atdUtil::IOException);

    void initStatistics();

    void calcStatistics(unsigned long periodMsec);

    size_t getMaxSampleLength() const
    	{ return maxSampleLength[currStatsIndex]; }
    size_t getMinSampleLength() const
    	{ return minSampleLength[currStatsIndex]; }

    int getReadErrorCount() const
    	{ return readErrorCount[0]; }
    int getCumulativeReadErrorCount() const
    	{ return readErrorCount[1]; }

    int getWriteErrorCount() const
    	{ return writeErrorCount[0]; }
    int getCumulativeWriteErrorCount() const
    	{ return writeErrorCount[1]; }

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

    std::list<Variable*> variables;

    std::list<const Variable*> constVariables;

    /**
     * Must be called before invoking readSamples(). Derived
     * classes should call initBuffer in their 
     * open() method.
     */
    void initBuffer() throw();

    /**
     * Delete the sensor buffer.  Derived classes should call
     * destroyBuffer in their close() method.
     */
    void destroyBuffer() throw();

    std::string devname;

    int id;

    const int BUFSIZE;
    char* buffer;
    int bufhead;
    int buftail;
                                                                                
    Sample* samp;
    size_t sampDataToRead;
    char* sampDataPtr;

    /**
     * DSMSensor maintains some counters that can be queried
     * to provide the current status.
     */
    time_t initialTimeSecs;
    size_t minSampleLength[2];
    size_t maxSampleLength[2];
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

}

#endif
