
#ifndef _DSM_ITEM_H
#define _DSM_ITEM_H

#include "NidasItem.h"
#include <nidas/core/DSMConfig.h>
#include <xercesc/dom/DOMNode.hpp>

using namespace nidas::core;


class DSMItem : public NidasItem
{
    Q_OBJECT

public:
    DSMItem(DSMConfig *dsm, int row, NidasModel *model, NidasItem *parent = 0) :
        NidasItem(dsm,row,model,parent) {}

    ~DSMItem();

    bool removeChild(NidasItem *item);

//protected: commented while Document still uses these

        // get/convert to the underlying model pointers
    DSMConfig *getDSMConfig() const {
     if (nidasType == DSMCONFIG)
         return reinterpret_cast<DSMConfig*>(this->nidasObject);
     else return 0;
     }

    xercesc::DOMNode* getDOMNode() {
        if (domNode)
          return domNode;
        else return domNode=findDOMNode();
        }

    // Seems we don't know how to use the operator here
    //operator DSMConfig*() const { if (nidasType == DSMCONFIG)  return reinterpret_cast<DSMConfig*>(this->nidasObject); else return 0;}
    //operator xercesc::DOMNode*() { if (domNode) return domNode; else return domNode=findDOMNode(); }

protected:
    xercesc::DOMNode *findDOMNode() const;
};

#endif
