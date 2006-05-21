/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <Sample.h>

using namespace dsm;

#ifdef MUTEX_PROTECT_REF_COUNTS
/* static */
atdUtil::Mutex SampleBase::refLock;
#endif

/* static */
int SampleBase::nsamps = 0;

Sample* dsm::getSample(sampleType type, size_t len)
{
    Sample* samp = 0;
    switch(type) {
    case CHAR_ST:
	samp = getSample<char>(len);
	break;
    case UCHAR_ST:
	samp = getSample<unsigned char>(len);
	break;
    case SHORT_ST:
	len /= sizeof(short);
	samp = getSample<short>(len);
	break;
    case USHORT_ST:
	len /= sizeof(short);
	samp = getSample<unsigned short>(len);
	break;
    case LONG_ST:
	len /= sizeof(long);
	samp = getSample<long>(len);
	break;
    case ULONG_ST:
	len /= sizeof(long);
	samp = getSample<unsigned long>(len);
	break;
    case FLOAT_ST:
	len /= sizeof(float);
	samp = getSample<float>(len);
	break;
    case DOUBLE_ST:
	len /= sizeof(double);
	samp = getSample<double>(len);
	break;
    case LONG_LONG_ST:
	len /= sizeof(long long);
	samp = getSample<long long>(len);
	break;
    case UNKNOWN_ST:
	return samp;
	break;
    }
    samp->setDataLength(len);
    return samp;
}
