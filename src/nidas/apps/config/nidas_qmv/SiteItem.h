
#ifndef _SITE_ITEM_H
#define _SITE_ITEM_H

#include "NidasItem.h"
#include <nidas/core/Site.h>
#include <xercesc/dom/DOMNode.hpp>

using namespace nidas::core;


class SiteItem : public NidasItem
{

public:
    SiteItem(Site *site, int row, NidasModel *model, NidasItem *parent = 0) :
        NidasItem(site,row,model,parent) {}

    ~SiteItem();

    bool removeChild(NidasItem *item);

//protected: commented while Document still uses these

        // get/convert to the underlying model pointers
    Site *getSite() const {
     if (nidasType == SITE)
         return reinterpret_cast<Site*>(this->nidasObject);
     else return 0;
     }

    xercesc::DOMNode* getDOMNode() {
        if (domNode)
          return domNode;
        else return domNode=findDOMNode();
        }

    // Seems we don't know how to use the operator here
    //operator Site*() const { if (nidasType == SITE)  return reinterpret_cast<Site*>(this->nidasObject); else return 0;}
    //operator xercesc::DOMNode*() { if (domNode) return domNode; else return domNode=findDOMNode(); }

protected:
    xercesc::DOMNode *findDOMNode() const;
    QString name();
};

#endif
