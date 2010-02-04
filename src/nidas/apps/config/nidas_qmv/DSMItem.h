
#ifndef _DSM_ITEM_H
#define _DSM_ITEM_H

#include "NidasItem.h"
#include <nidas/core/DSMConfig.h>
#include <xercesc/dom/DOMNode.hpp>

using namespace nidas::core;


class DSMItem : public NidasItem
{
    DSMItem(DSMConfig *dsm, int row, NidasItem *parent = 0) : NidasItem(dsm,row,parent) {}
    DSMConfig *getDSMConfig() { return static_cast<DSMConfig*>(nidasObject); }
    DOMNode *getDOMNode();
}

#endif
