//
//              Copyright 2005 UCAR, NCAR, All Rights Reserved
//

#ifndef DSM_SORTEDSAMPLESET_H
#define DSM_SORTEDSAMPLESET_H

#include <Sample.h>

#include <set>

namespace dsm {

class SampleTimetagComparator {
public:
  bool operator() (const Sample* x, const Sample *y) const {
    // return true if x is less than y
    int d = x->getTimeTag() - y->getTimeTag();
    if (d < 0) {
      if (d < -86100000) return false;	// x has rolled over back to 0
      return true;
    }
    else {
      if (d > 86100000) return true;	// y has rolled over back to 0
      return false;
    }
  }
};

class SortedSampleSet: public std::multiset<const Sample*,SampleTimetagComparator> {};

}

#endif
