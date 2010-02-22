
#ifndef _SAMPLE_ITEM_H
#define _SAMPLE_ITEM_H

#include "NidasItem.h"
#include <nidas/core/SampleTag.h>

using namespace nidas::core;


class SampleItem : public NidasItem
{

public:
    SampleItem(SampleTag *sampleTag, int row, NidasModel *model, NidasItem *parent = 0) :
        NidasItem(sampleTag,row,model,parent) {}

    ~SampleItem();

    bool removeChild(NidasItem *item) { return false; } // XXX

    std::string devicename() { return this->dataField(1).toStdString(); }


protected:
        // get/convert to the underlying model pointers
    SampleTag *getSample() const {
     if (nidasType == SAMPLE)
         return reinterpret_cast<SampleTag*>(this->nidasObject);
     else return 0;
     }

};

#endif
