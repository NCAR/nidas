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
