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
#include <IODevice.h>
#include <SampleScanner.h>
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
 * Much of the implementation of a DSMSensor is delegated
 * to an IODevice and a SampleScanner, which are
 * passed in the constructor.
 * DSMSensor can fill in its attributes
 * from an XML DOM element with fromDOMElement().
 * One attribute of a DSMSensor is the system device
 * name associated with this sensor, e.g. "/dev/xxx0".
 * Once a device name has been set, then a user of this sensor
 * can call open(),  and then ioctl(), read() and write(),
 * which are carried out by the IODevice.
 *
 * SampleClient's can call
 * addRawSampleClient()/removeRawSampleClient() if they want to
 * receive raw Samples from this sensor.
 *
 * SampleClient's can also call
 * addSampleClient()/removeSampleClient() if they want to
 * receive (minimally) processed Samples from this sensor.
 *
 * A common usage of a DSMSensor is to add it to a PortSelector
 * object with PortSelector::addSensorPort().
 * When the PortSelector::run method has determined that there is data
 * available on a DSMSensor's file descriptor, it will then call
 * the readSamples() method which reads the samples from the
 * IODevice, and forwards the raw and processed
 * samples to all associated SampleClient's of this DSMSensor.
 *
 */
class DSMSensor : public RawSampleSource, public SampleSource,
	public SampleClient, public DOMable {

public:

    /**
     * Constructor.
     * @param scanner Pointer to a SampleScanner to be used
     *        for scanning raw data from the sensor into Samples.
     * If this DSMSensor is being used in post-processing,
     * then the scanner can be a null pointer(0).
     * After construction, DSMSensor owns the SampleScanner.
     */

    DSMSensor();

    virtual ~DSMSensor();

    /**
     * Set the DSMConfig for this sensor.
     */
    void setDSMConfig(const DSMConfig* val)
    {
        dsm = val;
    }

    /**
     * What DSMConfig am I associated with?
     */
    const DSMConfig* getDSMConfig() const
    {
        return dsm;
    }

    /**
     * Set the name of the system device that the sensor
     * is connected to.
     * @param val Name of device, e.g. "/dev/xxx0".
     */
    virtual void setDeviceName(const std::string& val)
    {
       devname = val;
    }

    /**
     * Fetch the name of the system device that the sensor
     * is connected to.
     */
    virtual const std::string& getDeviceName() const
    {
	return devname;
    }

    /**
     * Set the class name. In the usual usage this
     * method is not used, and getClassName() is
     * over-ridden in a derived class to return
     * a constant string.
     */
    virtual void setClassName(const std::string& val)
    {
        className = val;
    }

    /**
     * Fetch the class name.
     */
    virtual const std::string& getClassName() const
    {
        return className;
    }

    /**
     * Set the name of the catalog entry for this sensor.
     */
    virtual void setCatalogName(const std::string& val)
    {
        catalogName = val;
    }

    /**
     * Fetch the name of the catalog entry for this sensor.
     */
    virtual const std::string& getCatalogName() const 
    {
        return catalogName;
    }

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
    virtual std::string getName() const
    {
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

    virtual int getReadFd() const
    {
	if (iodev) return iodev->getReadFd();
	return -1;
    }

    virtual int getWriteFd() const
    {
	if (iodev) return iodev->getWriteFd();
	return -1;
    }

    /**
     * Whether to reopen this sensor on an IOException.
     * The base method returns true.  Over-ride if
     * a reopen should not be attempted.
     */
    virtual bool reopenOnIOException() const { return true; }

    /**
     * Set the various levels of the sensor identification.
     * A sensor ID is a 32-bit value comprised of four parts:
     * 6-bit not used, 10-bit DSM id, and 16-bit sensor+sample ids
     */
    void setId(dsm_sample_id_t val) { id = SET_FULL_ID(id,val); }
    void setShortId(unsigned short val) { id = SET_SHORT_ID(id,val); }
    void setDSMId(unsigned short val) { id = SET_DSM_ID(id,val); }

    /**
     * Get the various levels of the samples identification.
     * A sample tag ID is a 32-bit value comprised of four parts:
     * 6-bit type_id  10-bit DSM_id  16-bit sensor+sample
     */
    dsm_sample_id_t  getId()      const { return GET_FULL_ID(id); }
    unsigned short getDSMId()   const { return GET_DSM_ID(id); }
    unsigned short getShortId() const { return GET_SHORT_ID(id); }

    /**
     * Set desired latency, providing some control
     * over the response time vs buffer efficiency tradeoff.
     * Setting a latency of 1/10 sec means buffer
     * data in the driver for a 1/10 sec, then send the data
     * to user space. As implemented here, it must be
     * set before doing a sensor open().
     * @param val Latency, in seconds.
     */
    void setLatency(float val)
    	throw(atdUtil::InvalidParameterException)
    {
        latency = val;
    }

    float getLatency() const { return latency; }

    virtual SampleDater::status_t setSampleTime(SampleDater* dater,Sample* samp)
    {
        return dater->setSampleTime(samp);
    }

    /**
     * Factory method for an IODevice for this DSMSensor.
     * Must be implemented by derived classes.
     */
    virtual IODevice* buildIODevice() throw(atdUtil::IOException) = 0;

    /**
     * Factory method for a SampleScanner for this DSMSensor.
     * Must be implemented by derived classes.
     */
    virtual SampleScanner* buildSampleScanner() = 0;

    /**
    * Open the device. flags are a combination of O_RDONLY, O_WRONLY.
    */
    virtual void open(int flags)
    	throw(atdUtil::IOException,atdUtil::InvalidParameterException);

    /**
     * Initialize the DSMSensor. If the DSMSensor is
     * not being opened (as in post-realtime processing)
     * then the init() method will be called before the
     * first call to process. Either open() or init() will
     * be called after setting the required properties,
     * and before calling readSamples(), receive(), or process().
     */
    virtual void init() throw(atdUtil::InvalidParameterException);

    /**
     * How do I want to be opened.  The user can ignore it if they want to.
     * @return One of O_RDONLY, O_WRONLY or O_RDWR.
     */
    virtual int getDefaultMode() const { return O_RDONLY; }

    /**
     * Read from the device (duh). Behaves like the read(2) system call,
     * without a file descriptor argument, and with an IOException.
     */
    virtual size_t read(void *buf, size_t len)
    	throw(atdUtil::IOException)
    {
        return iodev->read(buf,len);
    }

    /**
     * Write to the device (duh). Behaves like write(2) system call,
     * without a file descriptor argument, and with an IOException.
     */
    virtual size_t write(const void *buf, size_t len)
    	throw(atdUtil::IOException)
    {
        return iodev->write(buf,len);
    }

    /**
     * Perform an ioctl on the device. request is an integer
     * value which must be supported by the device. Normally
     * this is a value from a header file for the device.
     */
    virtual void ioctl(int request, void* buf, size_t len)
    	throw(atdUtil::IOException)
    {
        iodev->ioctl(request,buf,len);
    }

    /**
     * close my associated device.
     */
    virtual void close() throw(atdUtil::IOException);

    /**
     * Read raw samples from my associated file descriptor,
     * and distribute() them to my RawSampleClient's.
     *
     * The DSMSensor implementation of readSamples() assumes
     * that the data read from the file descriptor is
     * formatted into samples in the format of a
     * struct dsm_sample, i.e. a 4 byte unsigned integer
     * time-tag (milliseconds since midnight GMT), followed
     * by a 4 byte unsigned integer data length, and then
     * length number of bytes of data.
     */
    virtual dsm_time_t readSamples(SampleDater* dater)
    	throw(atdUtil::IOException) 
    {
        return scanner->readSamples(this,dater);
    }

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

    void printStatusHeader(std::ostream& ostr) throw();
    virtual void printStatus(std::ostream&) throw();
    void printStatusTrailer(std::ostream& ostr) throw();

    /**
     * Update the sensor sampling statistics.  Should be called
     * every periodUsec by a user of this sensor.
     * @param periodUsec Statistics period.
     */
    void calcStatistics(unsigned long periodUsec)
    {
        if (scanner) scanner->calcStatistics(periodUsec);
    }

    size_t getMaxSampleLength() const
    {
	if (scanner) return scanner->getMaxSampleLength();
	return 0;
    }

    size_t getMinSampleLength() const
    {
        if (scanner) return scanner->getMinSampleLength();
	return 0;
    }

    float getObservedSamplingRate() const
    {
        if (scanner) return scanner->getObservedSamplingRate();
	return 0.0;
    }

    float getObservedDataRate() const
    {
        if (scanner) return scanner->getObservedDataRate();
	return 0.0;
    }

    size_t getBadTimeTagCount() const
    {
	if (scanner) return scanner->getBadTimeTagCount();
	return 0;
    }

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

    /**
     * Utility function for replacing backslash sequences in a string.
     *  \\n=newline, \\r=carriage-return, \\t=tab, \\\\=backslash
     *  \\xhh=hex, where hh are (exactly) two hex digits and
     *  \\000=octal, where 000 are exactly three octal digits.
     */
    static std::string replaceBackslashSequences(std::string str);

    /* note that the above back slashes above are doubled so that
     * doxygen displays them as one back slash.  One does
     * not double them in the parameter string.
     */

    /**
     * Utility function for substituting backslash sequences back
     * into a string.
     */
    static std::string addBackslashSequences(std::string str);

protected:

    IODevice* getIODevice() const { return iodev; }

    SampleScanner* getSampleScanner() const { return scanner; }

    virtual const std::vector<SampleTag*>& getMySampleTags()
    	{ return sampleTags; }


private:

    std::string devname;

    /**
     * Class name attribute of this sensor. Only used here for
     * informative messages.
     */
    std::string className;

    std::string catalogName;

    /**
     * Sensor suffix, which is added to variable names.
     */
    std::string suffix;

    std::string location;

    IODevice* iodev;

    SampleScanner* scanner;

    const DSMConfig* dsm;

    /**
     * Id of this sensor.  Raw samples from this sensor will
     * have this id.
     */
    dsm_sample_id_t id;

protected:

    std::vector<SampleTag*> sampleTags;

private:

    std::vector<const SampleTag*> constSampleTags;

    float latency;

private:
    // no copying
    DSMSensor(const DSMSensor& x);

    // no assignment
    DSMSensor& operator=(const DSMSensor& x);

    // toggle flag for zebra striping printStatus
    static bool zebra;

};

}

#endif
