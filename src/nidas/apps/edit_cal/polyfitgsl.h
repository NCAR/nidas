/**
 * @link http://rosettacode.org/wiki/Polynomial_regression
 */
#ifndef _POLYFITGSL_H
#define _POLYFITGSL_H
bool polynomialfit(int obs, int order, 
		   double *dx, double *dy, double *store); /* n, p */
#endif
