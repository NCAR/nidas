/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <Sample.h>
// #include <SamplePool.h>

using namespace dsm;

/* static */
int SampleBase::nsamps = 0;

Sample* dsm::getSample(sampleType type, size_t len)
{
    Sample* samp = 0;
    switch(type) {
    case CHAR_ST:
	// samp = SamplePool<SampleT<char> >::getInstance()->getSample(len);
	samp = getSample<char>(len);
	break;
    case UCHAR_ST:
	// samp = SamplePool<SampleT<unsigned char> >::getInstance()->getSample(len);
	samp = getSample<unsigned char>(len);
	break;
    case SHORT_ST:
	len /= sizeof(short);
	// samp = SamplePool<SampleT<short> >::getInstance()->getSample(len);
	samp = getSample<short>(len);
	break;
    case USHORT_ST:
	len /= sizeof(short);
	// samp = SamplePool<SampleT<unsigned short> >::getInstance()->getSample(len);
	samp = getSample<unsigned short>(len);
	break;
    case LONG_ST:
	len /= sizeof(long);
	// samp = SamplePool<SampleT<long> >::getInstance()->getSample(len);
	samp = getSample<long>(len);
	break;
    case ULONG_ST:
	len /= sizeof(long);
	// samp = SamplePool<SampleT<unsigned long> >::getInstance()->getSample(len);
	samp = getSample<unsigned long>(len);
	break;
    case FLOAT_ST:
	len /= sizeof(float);
	// samp = SamplePool<SampleT<float> >::getInstance()->getSample(len);
	samp = getSample<float>(len);
	break;
    case DOUBLE_ST:
	len /= sizeof(double);
	// samp = SamplePool<SampleT<double> >::getInstance()->getSample(len);
	samp = getSample<double>(len);
	break;
    case LONG_LONG_ST:
	len = sizeof(long long);
	// samp = SamplePool<SampleT<long long> >::getInstance()->getSample(len);
	samp = getSample<long long>(len);
	break;
    case UNKNOWN_ST:
	return samp;
	break;
    }
    samp->setDataLength(len);
    return samp;
}
