/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-23 12:30:55 -0600 (Tue, 23 May 2006) $

    $LastChangedRevision: 3364 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/dynld/SampleInputStream.h $
 ********************************************************************
*/

#ifndef NIDAS_DYNLD_ISFF_PACKETINPUTSTREAM_H
#define NIDAS_DYNLD_ISFF_PACKETINPUTSTREAM_H

#include <nidas/dynld/isff/Packets.h>
#include <nidas/dynld/SampleInputStream.h>

namespace nidas { namespace dynld { namespace isff {

using namespace nidas::dynld;	// put this within namespace block


class PacketInputStream: public nidas::dynld::SampleInputStream
{

public:

    /**
     * Constructor.
     * @param iochannel The IOChannel that we use for data input.
     *   SampleInputStream will own the pointer to the IOChannel,
     *   and will delete it in ~SampleInputStream(). If 
     *   it is a null pointer, then it must be set within
     *   the fromDOMElement method.
     */
    PacketInputStream(IOChannel* iochannel = 0)
	    throw(nidas::util::InvalidParameterException);

    /**
     * Copy constructor, with a new, connected IOChannel.
     */
    PacketInputStream(const PacketInputStream& x,IOChannel* iochannel);

    /**
     * Create a clone, with a new, connected IOChannel.
     */
    virtual PacketInputStream* clone(IOChannel* iochannel);

    virtual ~PacketInputStream();

    std::string getName() const;

    const std::set<const SampleTag*>& getSampleTags() const;

    void init() throw();

    /**
     * Read the next sample from the InputStream. The caller must
     * call freeReference on the sample when they're done with it.
     * This method may perform zero or more reads of the IOChannel.
     */
    Sample* readSample() throw(nidas::util::IOException)
    {
        throw nidas::util::IOException(getName(),"readSample","not supported");
    }

    /**
     * Read a buffer of data, serialize the data into samples,
     * and distribute() samples to the receive() method of my
     * SampleClients and DSMSensors.
     * This will perform only one physical read of the underlying
     * IOChannel and so is appropriate to use when a select()
     * has determined that there is data available on our file
     * descriptor.
     */
    void readSamples() throw(nidas::util::IOException);

    void close() throw(nidas::util::IOException);

private:

    void collectSampleTags() throw(nidas::util::InvalidParameterException);

    const SampleTag* findSampleTag(int configId, int goesId, int sampleId)
	    throw(nidas::util::InvalidParameterException);

    int getXmitOffset(const SampleTag* tag);

    class GOESProject
    {
    public:
	GOESProject(Project*p): project(p) {}
	~GOESProject();

	Project* getProject() const { return project; }

	/**
	 * Get the station number, corresponding to a GOES id.
	 * @return -1: goes id not found.
	 * Throws InvalidParameterException if there is
	 * no "goes_ids" integer parameter for the Project.
	 */
	int getStationNumber(unsigned long goesId)
		throw(nidas::util::InvalidParameterException);

	int getXmitOffset(int stationNumber)
		throw(nidas::util::InvalidParameterException);

	/**
	 * Get a new SampleTag*, corresponding to station and sampleid.
	 * @return 0: Site for stationNumber not found, or sample for
	 *            with given sampleid not found.
	 * Throws InvalidParameterException if there is
	 * no "goes_ids" integer parameter for the Project.
	 */
	const SampleTag* getSampleTag(int stationNumber, int sampleId);

	const std::set<const SampleTag*>& getSampleTags();

	unsigned long getGOESId(int stationNum)
	    throw(nidas::util::InvalidParameterException);

    private:
	GOESProject(const GOESProject& x); 	// no copying
	
	GOESProject& operator=(const GOESProject& x) const; 	// no assign

	void readGOESIds()
	    throw(nidas::util::InvalidParameterException);

	Project* project;

	std::vector<unsigned long> goesIds;

	std::map<unsigned long,int> stationNumbersById;

	std::map<dsm_sample_id_t,SampleTag*> sampleTagsById;

	std::vector<int> xmitOffsets;

	std::set<const SampleTag*> sampleTags;


    };

    /**
     * Don't copy.
     */
    PacketInputStream(const PacketInputStream&);

    IOChannel* iochan;

    IOStream* iostream;

    PacketParser* packetParser;

    std::set<const SampleTag*> sampleTags;

    std::map<int,GOESProject*> projectsByConfigId;

    std::map<const SampleTag*,int> xmitOffsetsByTag;

    std::vector<SampleTag*> goesTags;

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
