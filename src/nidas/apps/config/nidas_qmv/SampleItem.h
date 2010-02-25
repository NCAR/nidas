
#ifndef _SAMPLE_ITEM_H
#define _SAMPLE_ITEM_H

#include "NidasItem.h"
#include "VariableItem.h"
#include <nidas/core/SampleTag.h>

using namespace nidas::core;


class SampleItem : public NidasItem
{

public:
    SampleItem(SampleTag *sampleTag, int row, NidasModel *theModel, NidasItem *parent = 0) ;

    ~SampleItem();

    NidasItem * child(int i);

    bool removeChild(NidasItem *item) { return false; } // XXX

    const QVariant & childLabel(int column) const { return NidasItem::_Variable_Label; }
    int childColumnCount() const {return 1;}

    std::string devicename() { return this->dataField(1).toStdString(); }

    QString dataField(int column);

protected:
        // get/convert to the underlying model pointers
    SampleTag *getSample() const { return _sampleTag; }

     QString name();

private:
    SampleTag * _sampleTag;

};

#endif
