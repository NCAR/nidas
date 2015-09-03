/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2012, Copyright University Corporation for Atmospheric Research
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
#ifndef _numeric_PolyEval_h_
#define _numeric_PolyEval_h_

#include <vector>

namespace numeric
{

inline double PolyEval(double *cof, unsigned int order, double target)
{
  if (order == 0)
    return 0.0;

  int corder = order - 1;

  double out = cof[corder];

  for (unsigned int k = 1; k < order; k++)
    out = cof[corder-k] + target * out;

  return out;
}

inline double PolyEval(std::vector<double> cof, double target)
{
  return PolyEval(&cof[0], cof.size(), target);
}

}

#endif
