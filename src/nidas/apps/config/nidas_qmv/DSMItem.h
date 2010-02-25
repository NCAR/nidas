
#ifndef _DSM_ITEM_H
#define _DSM_ITEM_H

#include "NidasItem.h"
#include <nidas/core/DSMConfig.h>
#include <xercesc/dom/DOMNode.hpp>

#include <iostream>
#include <fstream>

using namespace nidas::core;


class DSMItem : public NidasItem
{

public:
    DSMItem(DSMConfig *dsm, int row, NidasModel *theModel, NidasItem *parent = 0); 

    ~DSMItem();

    NidasItem * child(int i);

    bool removeChild(NidasItem *item);

    QString dataField(int column);
    const QVariant & childLabel(int column) const;

    int childColumnCount() const {return 4;}
//protected: commented while Document still uses these

        // get/convert to the underlying model pointers
    DSMConfig *getDSMConfig() const { return _dsm; }

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
    QString name();

private:
    DSMConfig * _dsm;
};

#endif
