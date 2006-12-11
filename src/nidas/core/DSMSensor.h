/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/
#ifndef NIDAS_CORE_DSMSENSOR_H
#define NIDAS_CORE_DSMSENSOR_H

#include <nidas/core/SampleClient.h>
#include <nidas/core/SampleSource.h>
#include <nidas/core/IODevice.h>
#include <nidas/core/SampleScanner.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/DOMable.h>
#include <nidas/core/dsm_sample.h>

#include <nidas/util/IOException.h>
#include <nidas/util/InvalidParameterException.h>

#include <string>
#include <list>

#include <fcntl.h>

namespace nidas { namespace core {

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
 * A common usage of a DSMSensor is to add it to a SensorHandler
 * object with SensorHandler::addSensorPort().
 * When the SensorHandler::run method has determined that there is data
 * available on a DSMSensor's file descriptor, it will then call
 * the readSamples() method which reads the samples from the
 * IODevice, and forwards the raw and processed
 * samples to all associated SampleClient's of this DSMSensor.
 *
 */
class DSMSensor : public SampleSource, public SampleClient,
	public DOMable {

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
     * What Site am I associated with?
     */
    const Site* getSite() const;

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
     * It is only necessary to have a sensor suffix
     * to make variable names unique.  For example, if
     * there are multiple sensors at the same height,
     * with common variable names, then one can
     * use a sensor suffix to make unique names.
     * The sensor suffix is the second dot-separated
     * word in a variable name, where only the first
     * word is required:
     *  variable[.sensor][.height][.site]
     */
    const std::string& getSuffix() const { return suffix; }

    void setSuffix(const std::string& val);

    /**
     * Set sensor height above ground via a string which is added
     * to variable names. 
     * @param val String containing sensor height and units
     * in meters (m) or centimeters(cm), e.g. "15m".
     * This height string is added to all the variable names,
     * with a "." separator, so that a variable "u" for this
     * sensor becomes "u.15m".
     */
    void setHeight(const std::string& val);

    /**
     * Set sensor height above ground.
     * @param val height above ground, in meters.
     * The height is added to all the variable names,
     * with a "." separator, so that a variable "u" for this
     * sensor becomes "u.15m".
     */
    void setHeight(float val);

    /**
     * Get sensor height above ground in a string.
     * @return Height of sensor above ground,  e.g. "15m".
     */
    const std::string& getHeightString() const { return heightString; }

    /**
     * Get sensor height above ground.
     * @return Height of sensor above ground, in meters. Nan if unknown.
     */
    float getHeight() const { return height; }

    /**
     * Set sensor depth below ground via a string which is added
     * to variable names. 
     * @param val String containing sensor below and units
     * in meters (m) or centimeters(cm), e.g. "5cm".
     * This depth string is added to all the variable names,
     * with a "." separator, so that a variable "Tsoil" for this
     * sensor becomes "Tsoil.5cm".
     */
    void setDepth(const std::string& val);

    /**
     * Set sensor depth below ground.
     * @param val depth below ground, in meters.
     * The depth is converted to centimeters and is added
     * to all the variable names, with a "." separator, so
     * that a variable "Tsoil" for this
     * sensor becomes "Tsoil.5cm".
     */
    void setDepth(float val);

    /**
     * Get sensor depth below ground in a string.
     * @return Depth of sensor below ground,  e.g. "5cm".
     */
    const std::string& getDepthString() const { return depthString; }

    /**
     * Get sensor depth below ground.
     * @return Depth of sensor below ground, in meters.
     */
    float getDepth() const { return -height; }

    /**
     * Full sensor suffix, the concatenation of the
     * sensor suffix, if any, and the height or depth
     * string, if any.  The full sensor suffix are
     * words 2 and 3 in the dot-separated name:
     *  variable[.sensor][.height][.site]
     */
    const std::string& getFullSuffix() const { return fullSuffix; }

    /**
     * Implementation of SampleSource::getSampleTags().
     */
    virtual const std::set<const SampleTag*>& getSampleTags() const
    {
        return constSampleTags;
    }

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
     * 6-bit sample type id (not used by DSMSensor), 10-bit DSM id,
     * and 16-bit sensor+sample ids.
     */
    void setId(dsm_sample_id_t val) { id = SET_FULL_ID(id,val); }
    void setShortId(unsigned long val) { id = SET_SHORT_ID(id,val); }
    void setDSMId(unsigned long val) { id = SET_DSM_ID(id,val); }

    /**
     * Get the various levels of the samples identification.
     * A sample tag ID is a 32-bit value comprised of four parts:
     * 6-bit sample type id(not used by DSMSensor), a 10-bit DSM id,
     * 16-bit sensor+sample ids.
     */
    dsm_sample_id_t  getId()      const { return GET_FULL_ID(id); }
    unsigned long getDSMId()   const { return GET_DSM_ID(id); }
    unsigned long getShortId() const { return GET_SHORT_ID(id); }

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
    	throw(nidas::util::InvalidParameterException)
    {
        latency = val;
    }

    float getLatency() const { return latency; }

    /**
     * DSMSensor provides a SampleSource interface for its raw samples.
     */
    void addRawSampleClient(SampleClient* c) throw() {
        rawSource.addSampleClient(c);
    }

    /**
     * DSMSensor provides a SampleSource interface for its raw samples.
     */
    void removeRawSampleClient(SampleClient* c) throw() {
        rawSource.removeSampleClient(c);
    }

    /**
     * What is my raw sample?
     */
    const SampleTag* getRawSampleTag() const
    {
        return rawSampleTag;
    }

    /**
     * Add a parameter to this DSMSensor. DSMSensor
     * will then own the pointer and will delete it
     * in its destructor. If a Parameter exists with the
     * same name, it will be replaced with the new Parameter.
     */
    void addParameter(Parameter* val);

    /**
     * Get list of parameters.
     */
    const std::list<const Parameter*>& getParameters() const
    {
	return constParameters;
    }

    /**
     * Fetch a parameter by name. Returns a NULL pointer if
     * no such parameter exists.
     */
    const Parameter* getParameter(const std::string& name) const;

    /**
     * Distribute a sample to my clients. Calls receive() method
     * of each client, passing the pointer to the Sample.
     */
    void distributeRaw(const Sample* s) throw()
    {
        rawSource.distribute(s);
    }

    /**
     * Factory method for an IODevice for this DSMSensor.
     * Must be implemented by derived classes.
     */
    virtual IODevice* buildIODevice() throw(nidas::util::IOException) = 0;

    /**
     * Factory method for a SampleScanner for this DSMSensor.
     * Must be implemented by derived classes.
     */
    virtual SampleScanner* buildSampleScanner() = 0;

    /**
    * Open the device. flags are a combination of O_RDONLY, O_WRONLY.
    */
    virtual void open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    /**
     * Initialize the DSMSensor. If the DSMSensor is
     * not being opened (as in post-realtime processing)
     * then the init() method will be called before the
     * first call to process. Either open() or init() will
     * be called after setting the required properties,
     * and before calling readSamples(), receive(), or process().
     */
    virtual void init() throw(nidas::util::InvalidParameterException);

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
    	throw(nidas::util::IOException)
    {
        return iodev->read(buf,len);
    }

    /**
     * Write to the device (duh). Behaves like write(2) system call,
     * without a file descriptor argument, and with an IOException.
     */
    virtual size_t write(const void *buf, size_t len)
    	throw(nidas::util::IOException)
    {
        return iodev->write(buf,len);
    }

    /**
     * Perform an ioctl on the device. request is an integer
     * value which must be supported by the device. Normally
     * this is a value from a header file for the device.
     */
    virtual void ioctl(int request, void* buf, size_t len)
    	throw(nidas::util::IOException)
    {
        iodev->ioctl(request,buf,len);
    }

    /**
     * close my associated device.
     */
    virtual void close() throw(nidas::util::IOException);

    /**
     * Read samples from my associated file descriptor,
     * and distribute() them to my RawSampleClient's.
     * This is a convienence method which does a
     * readBuffer() to read available data from the DSMSensor
     * into a buffer, and then repeatedly calls nextSample()
     * to extract all samples out of that buffer.
     */
    virtual dsm_time_t readSamples()
    	throw(nidas::util::IOException);

    /**
     * Read data from attached sensor into an internal buffer.
     */
    virtual size_t readBuffer() throw(nidas::util::IOException)
    {
        return scanner->readBuffer(this);
    }

    /**
     * Read data from attached sensor into an internal buffer.
     */
    virtual size_t readBuffer(int msecTimeout)
    	throw(nidas::util::IOException) 
    {
        return scanner->readBuffer(this,msecTimeout);
    }

    /**
     * Extract the next sample from the buffer. Returns a
     * null pointer if there are no samples left in the buffer.
     */
    virtual Sample* nextSample()
    {
        return scanner->nextSample(this);
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

    SampleTagIterator getSampleTagIterator() const;

    VariableIterator getVariableIterator() const;

    /**
     * Crawl through the DOM tree for a DSMSensor to find
     * the class name - scanning the catalog entry if
     * necessary.
     */
    static const std::string getClassName(const xercesc::DOMElement* node)
    	throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

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
    static std::string replaceBackslashSequences(const std::string& str);

    /* note that the above back slashes above are doubled so that
     * doxygen displays them as one back slash.  One does
     * not double them in the parameter string.
     */

    /**
     * Utility function for substituting backslash sequences back
     * into a string.
     */
    static std::string addBackslashSequences(const std::string& str);

protected:

    IODevice* getIODevice() const { return iodev; }

    SampleScanner* getSampleScanner() const { return scanner; }

    /**
     * Add a SampleTag to this sensor.
     * Throw an exception the DSMSensor cannot support
     * the sample (bad rate, wrong number of variables, etc).
     * DSMSensor will own the pointer.
     */
    virtual void addSampleTag(SampleTag* val)
    	throw(nidas::util::InvalidParameterException);

    /**
     * Get non-const SampleTag pointers.
     */
    virtual std::set<SampleTag*>& getncSampleTags() 
    {
        return sampleTags;
    }

    /**
     * What is my raw sample?
     */
    SampleTag* getncRawSampleTag() const
    {
        return rawSampleTag;
    }

    void setFullSuffix(const std::string& val) { fullSuffix = val; }

    /**
     * Set the calibration file for this DSMSensor. After this
     * method is finished, DSMSensor will own the pointer, and
     * will delete it in the DSMSensor destructor.
     */
    void setCalFile(CalFile* val)
    {
        calFile = val;
    }

    CalFile* getCalFile()
    {
        return calFile;
    }

private:

    std::string devname;

    IODevice* iodev;

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

    std::string heightString;

    std::string depthString;

    float height;

    /**
     * Concatenation of sensor suffix, and the height or depth string
     */
    std::string fullSuffix;    

    std::string location;

    SampleScanner* scanner;

    const DSMConfig* dsm;

    /**
     * Id of this sensor.  Raw samples from this sensor will
     * have this id.
     */
    dsm_sample_id_t id;

    std::set<SampleTag*> sampleTags;

    std::set<const SampleTag*> constSampleTags;

    SampleTag* rawSampleTag;

    /**
     * Used for the implementation of a SampleSource for
     * raw samples.
     */
    class RawSampleSource: public SampleSource
    {
    public:
	/**
	 * Must implement this.
	 */
	const std::set<const SampleTag*>& getSampleTags() const
	{
	    return tags;
	}
    private:
        std::set<const SampleTag*> tags;
    } rawSource;

    // toggle flag for zebra striping printStatus
    static bool zebra;

    float latency;

    /**
     * Map of parameters by name.
     */
    std::map<std::string,Parameter*> parameters;

    /**
     * List of const pointers to Parameters for providing via
     * getParameters().
     */
    std::list<const Parameter*> constParameters;

    CalFile* calFile;

private:
    // no copying
    DSMSensor(const DSMSensor& x);

    // no assignment
    DSMSensor& operator=(const DSMSensor& x);

};

}}	// namespace nidas namespace core

#endif
