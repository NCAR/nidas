/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include <nidas/core/AdaptiveDespiker.h>

#include <cmath>

#include <iostream>

using namespace nidas::core;
using namespace std;

bool AdaptiveDespiker::staticInitDone = AdaptiveDespiker::staticInit();

float AdaptiveDespiker::_adj[AdaptiveDespiker::ADJUST_TABLE_SIZE][2];

/*
 * Create table of discrimination levels, which are a function of
 * correlation.
 */
/* static */
bool AdaptiveDespiker::staticInit()
{
    float x[5],y[5],y2[5],ypn;

    /* Original table determined by Jorgen Hojstrup
     * from simulated time series */
    x[0] = 0.0;	y[0] = 1.;
    x[1] = 0.1;	y[1] = 1.;
    x[2] = 0.5;	y[2] = 0.89;
    x[3] = 0.9;	y[3] = 0.44;
    x[4] = 0.99;y[4] = 0.18;

    ypn = -y[3] / ( 1 - x[3]);

    spline(x,y,5,0.0,ypn,y2);

    double a=0;
    for (int i=0; i < ADJUST_TABLE_SIZE; i++,a+=1. / ADJUST_TABLE_SIZE ) {
      _adj[i][0] = a;
      _adj[i][1] = splint(x,y,y2,5,a);
    }
    return true;
}

AdaptiveDespiker::AdaptiveDespiker():
	_prob(1.e-5),_levelMultiplier(2.5),_maxMissingFreq(2.0),
    _u1(0.0), _mean1(0.0), _mean2(0.0), _var1(0.0), _var2(0.0), _corr(0.0),
    _initLevel(0.0), _level(0.0), _missfreq(0.0), _msize(0), _npts(0)
{
    setDiscLevelMultiplier(2.5);
    setOutlierProbability(1.e-5);
}

/*
 * From Jorgen Hojstrup's paper:
 * "The discrimination level is selected such that the probability
 * of exceeding this level is [_prob], assuming a normal distribution
 * of the difference between the forecast and the actual value.
 * In reality we will encounter a larger probability of exceeding
 * a certain level because the variance estimate is based on a limited
 * number of independent samples, and will therefore fluctuate around
 * the correct value.  Too large estimates for the variance will give
 * a smaller sensitivity for detection of errors and situations with
 * too small values will result in falsely detected errors."
 */
void AdaptiveDespiker::setOutlierProbability(float val)
{
    _prob = val;
    _level = _initLevel = discrLevel(_prob) * _levelMultiplier;
}

void AdaptiveDespiker::setDiscLevelMultiplier(float val)
{
    _levelMultiplier = val;
    _level = _initLevel = discrLevel(_prob) * _levelMultiplier;
}

/**
 * Copied from "Numerical Recipies in C", the only changes being
 * arrays are indexed from 0, due to personal taste, and doubles
 * are passed instead of floats.
 *
 * Given arrays a[0..n-1] and y[0..n-1] containing a tabulated function,
 * i.e. yi = f(xi), with  x1 < x2 < ... < xn-1 and given values yp1 and
 * ypn for the first derivative of the interpolating function at points
 * 1 and n-1 respectively, this routine returns an array y2[0..n-1] that
 * contains the second derivatives of the interpolatimng function at the
 * tabulated points xi.  If yp1 and/or ypn are equal to 1.e30 or larger,
 * the routine is signalled to set the corresponding boundary condition
 * for a natural spline, with zero second derivative at that boundary.
 */
/* static */
void AdaptiveDespiker::spline(float* x,float* y,int n,double yp1,double ypn,
  float* y2)
{
    int i,k;
    float p,qn,sig,un;

    float* u = new float[n-1];

    if (yp1 > .99e30) y2[0] = u[0] = 0.0;
    else {
      y2[0] = -0.5;
      u[0] = (3.0 / (x[1] - x[0])) * ((y[1] - y[0]) / (x[1] - x[0]) - yp1);
    }

    /*
     * This is the decomposition loop of the tridiagonal algorithm.
     * y2 and u are used for temporay storage of the decomposed factors.
     */
    for (i=1; i <  n-1; i++) {
	sig = (x[i]-x[i-1]) / (x[i+1] - x[i-1]);
	p = sig * y2[i-1] + 2.0;
	y2[i] = (sig - 1.0) / p;
	u[i] = (y[i+1] - y[i]) / (x[i+1] - x[i]) - (y[i] - y[i-1]) /
		(x[i] - x[i-1]);
	u[i] = (6.0 * u[i] / (x[i+1] - x[i-1]) - sig * u[i-1]) / p;
    }

    /*
     * The upper boundary condition is set either to be "natural" or else
     * to have a specified first derivative.
     */
    if (ypn > .99e30) qn = un = 0.0;
    else {
	qn = 0.5;
	un = (3.0 / (x[n-1] - x[n-2])) * (ypn - (y[n-1] - y[n-2]) /
		(x[n-1] - x[n-2]));
    }
    y2[n-1] = (un - qn * u[n-2]) / (qn * y2[n-2] + 1.0);

    /* This is the backsubstitution loop of the tridiagonal algorithim */
    for (k = n-2; k >= 0; k--) y2[k] = y2[k] * y2[k+1] + u[k];

    delete [] u;
}

/* Given the arrays xa[0..n-1] and ya[0..n-1], which tabulate a function
 * (with the xai's in order),a nd given the array y2a[0..n-1], which is the
 * output from spline above, and given a value of x, this routine returns a
 * cubic-spline interpolated value y.
 */
/* static */
double AdaptiveDespiker::splint( float* xa, float* ya, float* y2a,
  int n, double x) throw(std::range_error)
{
    int klo,khi,k;
    float h,b,a;
    double y;

    klo = 0;
    khi = n-1;

    while (khi-klo > 1) {
	k = (khi + klo) >> 1;
	if (xa[k] > x) khi = k;
	else klo = k;
    }

    h = xa[khi] - xa[klo];

    /* The xa's must be distinct */
    if (h == 0.0)
	throw std::range_error("Bad XA input to routine SPLINT");
    
    a = (xa[khi] - x) / h;
    b = (x - xa[klo]) / h;

    y = a * ya[klo] + b * ya[khi] +
	  (( a*a*a -a) * y2a[klo] + (b*b*b-b) * y2a[khi]) * (h*h)/6.0;

    return y;
}

void AdaptiveDespiker::reset()
{
    _npts = 0;
    _level = _initLevel;
    _missfreq = 0;
}

float AdaptiveDespiker::despike(float u,bool* spike)
{

    if (_npts <= STATISTICS_SIZE) {
	if (_npts == 0 ) initStatistics(u);
	else incrementStatistics(u);
	return u;
    }

    /*
     * If more than _maxMissingFreq of the recent 10 data points are
     * missing data points, don't substitute forecasted data or
     * update forecast statistics.
     */
    if (_missfreq > _maxMissingFreq) return u;

    float uf = forecast();

    /*
     * Check if current point is within the discrimination
     * level of the forecasted point.  If it isn't, replace the
     * data point, but don't update the statistics.
     */
    if (isnan(u) || fabs(u - uf) / sqrt(_var2) > _level) {
	u = uf;				/* If not, fix it */
	*spike = true;
    }
    else updateStatistics(u);
    return u;
}

void AdaptiveDespiker::initStatistics(float u)
{
    if (isnan(u)) {
        _missfreq = 0.1;
	return;
    }

    _missfreq = 0.0;

    /*
     * Initialize statistics
     * First point is repeated to compute mean1, var1 and corr 
     */
    _mean2 = _mean1 = u;
    _var2 = _var1 = _corr = u * u;
    _u1 = u;			/* Last point is saved in u1 */
    _npts++;
}

void AdaptiveDespiker::incrementStatistics(float u)
{
    if (isnan(u)) {
        _missfreq = _missfreq * 0.9 + 0.1;
	return;
    }

    _missfreq *= 0.9;

    _corr += u * _u1;		/* just a sum at this point */
    _mean2 += u;
    _mean1 += _u1;
    _var2 += u * u;
    _var1 += _u1 * _u1;
    _u1 = u;

    /* Finalize statistics */
    if (_npts++ == STATISTICS_SIZE) {
	_mean2 /= _npts;
	_mean1 /= _npts;
	_var2 = _var2 / _npts - _mean2 * _mean2;
	_var1 = _var1 / _npts - _mean1 * _mean1;
	if (_var1 < 0) _var1 = 0.;
	if (_var2 < 0) _var2 = 0.;

	_corr = (_corr / _npts - _mean2 * _mean1) / sqrt(_var1 * _var2);

	if ( _corr >  0.99) _corr =  0.99;
	else if ( _corr < -0.99) _corr = -0.99;

	if (fabs(_corr) <  1.e-10 && _corr != 0.0)
	    _corr *=  1.e-10 / fabs(_corr);

	_msize = STATISTICS_SIZE;
	_level = _initLevel * adjustLevel(fabs(_corr));
    }
}

void AdaptiveDespiker::updateStatistics(float u)
{
    if (isnan(u)) {
        _missfreq = _missfreq * 0.9 + 0.1;
	return;
    }
    _missfreq *= 0.9;

    /* Convert correlation back to un-normalized covariance */
    _corr *= sqrt(_var1 * _var2);

    double mx = (_msize - 1.) / _msize;
    _mean1 = _mean2;
    _mean2 = _mean2 * mx + u / _msize;
    _corr = _corr * mx + (u - _mean2) * (_u1 - _mean1) / _msize;
    _var1 = _var2;
    _var2 = _var2 * mx + (u - _mean2) * (u - _mean2) / _msize;
    if (_var2 < 0.) _var2 = 0.;
    double v1v2 = _var1*_var2;
    _corr = (v1v2 == 0.0) ? 1.0 : _corr/sqrt(v1v2);

    /*
     * Due to the running means approximation:
     *  m(n) = m(n-1) * (m-1)/m + x(n)/m
     * the correlation can be wrong, so make sure it at least
     * is within +-1.
     */
    if (_corr > 0.99) _corr = 0.99;
    else if (_corr < -0.99) _corr = -0.99;
    if ( fabs(_corr) <  1.e-10 && _corr != 0.0)
    	_corr *=  1.e-10 / fabs(_corr);

    /* Update memory size */
    if ( fabs(_corr) < .1 ) _msize = 100;
    else 
      _msize = std::min((size_t)rint(-230.2585 / log(fabs(_corr))),_npts);

    /* Every "so often" adjust discrimination level based on correlation */
    if (_npts % 25 == 0) 
      _level = _initLevel * adjustLevel(fabs(_corr));

    _npts++;
    _u1 = u;			/* save previous point */
}

/* static */
float AdaptiveDespiker::adjustLevel(float corr)
{
    int maxindex = ADJUST_TABLE_SIZE - 2;

    float incr = _adj[1][0] - _adj[0][0];

    int indx = (int)trunc((corr - _adj[0][0]) / incr);

    if (indx < 0) indx = 0;
    else if (indx > maxindex) indx = maxindex;

    double a = _adj[indx][1] +
	  (corr - _adj[indx][0]) * (_adj[indx+1][1] - _adj[indx][1]) / incr;

    return a;
}

/*
 * Find discrimination level for Gaussian for a given probability.
 * This needs the inverse of the erfc function.
 * The erfc function is inverted by interpolating a table of
 * results.
 */
/* static */
float AdaptiveDespiker::discrLevel(float prob)
{
    float ea[LEN_ERFC_ARRAY][2];
    
    double a;
    int i,i1,i2;

    /* Generate table of erfc(a * sqrt(2)) */
    for (a=0.,i=0; i < LEN_ERFC_ARRAY; i++,a+=.05) {
	ea[i][0] = a;
	ea[i][1] = erfc(a / 1.414214);
    }

    /* Determine closest two values in table for given probablility */
    /* erfc is monotonically decreasing */
    i1 = 0;
    i2 = LEN_ERFC_ARRAY -1;

    while (i2 > i1 + 1) {
       i = (i1 + i2) / 2;
       if (prob < ea[i][1]) i1 = i;
       else i2 = i;
    }
    /* i1 is index of value < prob, i2 is index of value >= prob */

    a = ea[i1][0] + (ea[i2][0] - ea[i1][0]) / (ea[i2][1] - ea[i1][1]) *
	  (prob - ea[i1][1]);
    // cerr << "prob=" << prob << " discrLevel=" << a << endl;
    return a;
}

