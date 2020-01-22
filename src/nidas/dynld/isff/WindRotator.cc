
#include "WindRotator.h"

#include <cmath>

using namespace nidas::dynld::isff;
using namespace std;


WindRotator::WindRotator(): _angle(0.0),_sinAngle(0.0),_cosAngle(1.0) 
{
}

double WindRotator::getAngleDegrees() const
{
    return _angle * 180.0 / M_PI;
}

void WindRotator::setAngleDegrees(double val)
{
    _angle = val * M_PI / 180.0;
    _sinAngle = ::sin(_angle);
    _cosAngle = ::cos(_angle);
}

void WindRotator::rotate(float* up, float* vp) const
{
    float u = (float)( *up * _cosAngle + *vp * _sinAngle);
    float v = (float)(-*up * _sinAngle + *vp * _cosAngle);
    *up = u;
    *vp = v;
}
