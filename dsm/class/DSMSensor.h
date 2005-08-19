/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/
#ifndef DSMSENSOR_H
#define DSMSENSOR_H

#include <SampleDater.h>
#include <SampleClient.h>
#include <SampleSource.h>
#include <RawSampleSource.h>
#include <SampleTag.h>
#include <DOMable.h>
#include <dsm_sample.h>

#include <atdUtil/IOException.h>
#include <atdUtil/InvalidParameterException.h>

#include <string>
#include <list>

#include <fcntl.h>


namespace dsm {

class DSMConfig;

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

    virtual ~DSMSensor();

    /**
     * Set the DSMConfig for this sensor.
     */
    void setDSMConfig(const DSMConfig* val) { dsm = val; }

    /**
     * What DSMConfig am I associated with?
     */
    const DSMConfig* getDSMConfig() const { return dsm; }

    /**
     * Set the name of the system device that the sensor
     * is connected to.
     * @param val Name of device, e.g. "/dev/xxx0".
     */
    virtual void setDeviceName(const std::string& val) { devname = val; }

    /**
     * Fetch the name of the system device that the sensor
     * is connected to.
     */
    virtual const std::string& getDeviceName() const { return devname; }

    /**
     * Set the class name of this sensor. The class name is
     * only used in informative messages - nothing else
     * is done with it.
     */
    void setClassName(const std::string& val) { classname = val; }

    /**
     * Fetch the class name.
     */
    virtual const std::string& getClassName() const { return classname; }

    /**
     * Fetch the DSM name.
     */
    const std::string& getDSMName() const;

    /**
     * Return a name that should fully identify this sensor. This
     * name could be used in informative messages. The returned name
     * has this format:
     *  dsmName:deviceName.
     */
    virtual std::string getName() const {
        return getDSMName() + ':' + getDeviceName();
    }

    /**
     * Location string. A DSMConfig also has a Location.
     * If the location has not been set for this DSMSensor,
     * then the location of the DSMConfig will be returned.
     */
    const std::string& getLocation() const;

    void setLocation(const std::string& val) { location = val; }

    /**
     * Sensor suffix, which is added to variable names.
     */
    const std::string& getSuffix() const { return suffix; }

    void setSuffix(const std::string& val) { suffix = val; }

    /**
     * Add a SampleTag to this sensor.
     * Throw an exception if you don't like the variables in the sample.
     */
    virtual void addSampleTag(SampleTag* var)
    	throw(atdUtil::InvalidParameterException);

    virtual const std::vector<const SampleTag*>& getSampleTags() const
    	{ return constSampleTags; }

    virtual int getReadFd() const = 0;

    virtual int getWriteFd() const = 0;

    /**
     * Set the various levels of the sensor identification.
     * A sensor ID is a 32-bit value comprised of four parts:
     * 6-bit type_id  10-bit DSM_id  16-bit sensor+sample
     */
    void setId(dsm_sample_id_t val) { id = SET_SAMPLE_ID(id,val); }
    void setShortId(unsigned short val) { id = SET_SHORT_ID(id,val); }
    void setDSMId(unsigned short val) { id = SET_DSM_ID(id,val); }

    /**
     * Get the various levels of the samples identification.
     * A sample tag ID is a 32-bit value comprised of four parts:
     * 6-bit type_id  10-bit DSM_id  16-bit sensor+sample
     */
    dsm_sample_id_t  getId()      const { return GET_SAMPLE_ID(id); }
    unsigned short getDSMId()   const { return GET_DSM_ID(id); }
    unsigned short getShortId() const { return GET_SHORT_ID(id); }

    /**
     * The SampleTags of this sensor also have ShortIds.
     * After they have been added to this DSMSensor, then
     * we need to update their ShortIds by adding the value
     * of the sensor ShortId.  We need to do this once,
     * after the SampleTags have been added to this sensor,
     * after the sensor id has been finally set, but before
     * an other objects use the ShortIds of the SampleTags.
     */
    void finalizeSampleIds();

    /**
    * Open the device. flags are a combination of O_RDONLY, O_WRONLY.
    */
    virtual void open(int flags) throw(atdUtil::IOException) = 0;

    /**
     * Initialize the DSMSensor. If the DSMSensor is
     * not being opened (as in post-realtime processing)
     * then the init() method will be called before the
     * first call to process. Either open() or init() will
     * be called after setting the required properties,
     * and before calling readSamples(), receive(), or process().
     */
    virtual void init() throw(atdUtil::IOException) {}

    /**
     * How do I want to be opened.  The user can ignore it if they want to.
     * @return One of O_RDONLY, O_WRONLY or O_RDWR.
     */
    virtual int getDefaultMode() const { return O_RDONLY; }

    /**
     * Set desired sensor latency. This can be implemented as a
     * way to improve buffering efficiency in the sensor driver.
     * Override this virtual method if a DSMSensor supports latency.
     * @param val Latency, in seconds.
     */
    virtual void setLatency(float val)
    	throw(atdUtil::InvalidParameterException)
    {
        throw atdUtil::InvalidParameterException(
		getName(),"latency","not supported");
    }

    virtual float getLatency() const { return 0.0; }

    /**
     * Read from the device (duh). Behaves like the read(2) system call,
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
     * close my associated device.
     */
    virtual void close() throw(atdUtil::IOException) = 0;

    /**
     * Read raw samples from my associated file descriptor,
     * and distribute() them to my RawSampleClient's.
     *
     * readSamples() assumes that the data read from
     * the file descriptor is formatted into samples
     * in the format of a struct dsm_sample, i.e. a
     * 4 byte unsigned integer time-tag (milliseconds since
     * midnight GMT), followed by a 4 byte unsigned integer data
     * length, and then length number of bytes of data.
     */
    dsm_time_t readSamples(SampleDater* dater)
    	throw(atdUtil::IOException);

    /**
     * A DSMSensor can be configured as a RawSampleClient
     * of itself, meaning it receives its own raw samples, and
     * applies further processing via its process method.
     */
    bool receive(const Sample *s) throw();

    /**
     * Apply further necessary processing to a raw sample
     * from this DSMSensor. Return the resultant sample(s)
     * in result.  The default implementation
     * of process() simply puts the input Sample into result.
     */
    virtual bool process(const Sample*,std::list<const Sample*>& result)
    	throw();

    static void printStatusHeader(std::ostream& ostr) throw();
    virtual void printStatus(std::ostream&) throw();
    static void printStatusTrailer(std::ostream& ostr) throw();

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

    size_t getBadTimeTagCount() const
    {
	return questionableTimeTags;
    }

    float getObservedSamplingRate() const;

    float getObservedDataRate() const;

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    virtual SampleDater::status_t setSampleTime(SampleDater* dater,Sample* samp)
    {
        return dater->setSampleTime(samp);
    }

    /**
     * Class name attribute of this sensor. Only used here for
     * informative messages.
     */
    std::string classname;

    const DSMConfig* dsm;

    std::string devname;

    std::string location;

    /**
     * Id of this sensor.  Raw samples from this sensor will
     * have this id.
     */
    dsm_sample_id_t id;

    std::vector<SampleTag*> sampleTags;

    std::vector<const SampleTag*> constSampleTags;

    bool sampleIdsFinalized;

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
    int readErrorCount[2];     // [0] is recent, [1] is cumulative
    int writeErrorCount[2];    // [0] is recent, [1] is cumulative
    int currStatsIndex;
    int reportStatsIndex;
    size_t nsamples;
    size_t nbytes;
    size_t questionableTimeTags;

    /**
    * Observed number of samples per second.
    */
    float sampleRateObs;

    float dataRateObs;

    std::string suffix;
};

}

#endif
