
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

    bool removeChild(NidasItem *item);

    const QVariant & childLabel(int column) const { 
        if (column == 0) return NidasItem::_Variable_Label;
        if (column == 1) return NidasItem::_CalCoef_Label;
    }
    int childColumnCount() const {return 2;}

    //std::string devicename() { return this->dataField(1).toStdString(); }

    QString dataField(int column);

    xercesc::DOMNode* getDOMNode() {
        if (domNode)
          return domNode;
        else return domNode=findDOMNode();
        }

    std::string sSampleId() { return this->dataField(0).toStdString(); }

    SampleTag *getSampleTag() const { return _sampleTag; }

protected:
        // get/convert to the underlying model pointers

    xercesc::DOMNode *findDOMNode();

    QString name();

private:
    SampleTag * _sampleTag;

};

#endif
