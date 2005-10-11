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
    inline bool operator() (const Sample* x, const Sample *y) const {
	// return true if x is less than y
	return x->getTimeTag() < y->getTimeTag();
    }
};

class SortedSampleSet: public std::multiset<const Sample*,SampleTimetagComparator> {};

}

#endif
