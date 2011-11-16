// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef NIDAS_DYNLD_ISFF_PACKETINPUTSTREAM_H
#define NIDAS_DYNLD_ISFF_PACKETINPUTSTREAM_H

#include <nidas/dynld/isff/Packets.h>
#include <nidas/dynld/SampleInputStream.h>

namespace nidas {

namespace core {
class Sample;
class SampleTag;
class IOChannel;
class IOStream;
}

namespace dynld { namespace isff {

class GOESProject;

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
    PacketInputStream(nidas::core::IOChannel* iochannel = 0)
	    throw(nidas::util::InvalidParameterException);

    /**
     * Copy constructor, with a new, connected IOChannel.
     */
    PacketInputStream(const PacketInputStream& x,nidas::core::IOChannel* iochannel);

    /**
     * Create a clone, with a new, connected IOChannel.
     */
    virtual PacketInputStream* clone(nidas::core::IOChannel* iochannel);

    virtual ~PacketInputStream();

    std::string getName() const;

    std::list<const nidas::core::SampleTag*> getSampleTags() const;

    void init() throw();

    /**
     * Read the next sample from the InputStream. The caller must
     * call freeReference on the sample when they're done with it.
     * This method may perform zero or more reads of the IOChannel.
     */
    nidas::core::Sample* readSample() throw(nidas::util::IOException)
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

    const nidas::core::SampleTag* findSampleTag(int configId, int goesId, int sampleId)
	    throw(nidas::util::InvalidParameterException);

    const GOESProject* getGOESProject(int configid) const
    	throw(nidas::util::InvalidParameterException);

    nidas::core::IOChannel* _iochan;

    nidas::core::IOStream* _iostream;

    PacketParser* _packetParser;

    mutable std::map<int,GOESProject*> _projectsByConfigId;

    /**
     * No copy.
     */
    PacketInputStream(const PacketInputStream&);

    /**
     * No assignment.
     */
    PacketInputStream& operator=(const PacketInputStream&);

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
