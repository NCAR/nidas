
#ifndef _DSM_ITEM_H
#define _DSM_ITEM_H

#include "NidasItem.h"
#include <nidas/core/DSMConfig.h>
#include <xercesc/dom/DOMNode.hpp>

using namespace nidas::core;


class DSMItem : public NidasItem
{
public:
    DSMItem(DSMConfig *dsm, int row, NidasModel *model, NidasItem *parent = 0) :
        NidasItem(dsm,row,model,parent) {}

        // get/convert to the underlying model pointers
    //DSMConfig *getDSMConfig() const { return static_cast<DSMConfig*>(nidasObject); }
    operator DSMConfig*() const { return static_cast<DSMConfig*>(nidasObject); }

    operator xercesc::DOMNode*() { if (domNode) return domNode; else return domNode=findDOMNode(); }

protected:
    xercesc::DOMNode *findDOMNode() const;
};

#endif
