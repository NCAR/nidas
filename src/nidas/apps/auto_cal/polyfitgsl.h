/**
 * @link http://rosettacode.org/wiki/Polynomial_regression
 */
#ifndef _POLYFITGSL_H
#define _POLYFITGSL_H
#include <gsl/gsl_multifit.h>
#include <stdbool.h>
#include <math.h>
bool polynomialfit(int obs, int degree, 
		   double *dx, double *dy, double *store); /* n, p */
#endif
