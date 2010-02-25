
#ifndef _SITE_ITEM_H
#define _SITE_ITEM_H

#include "NidasItem.h"
#include "DSMItem.h"
#include <nidas/core/Site.h>
#include <xercesc/dom/DOMNode.hpp>

using namespace nidas::core;


class SiteItem : public NidasItem
{

public:
    SiteItem(Site *site, int row, NidasModel *theModel, NidasItem *parent = 0) ;

    ~SiteItem();

    NidasItem * child(int i);

    bool removeChild(NidasItem *item);

    QString dataField(int column);

    const QVariant & childLabel(int column) const { return NidasItem::_DSM_Label; }
    int childColumnCount() const {return 1;}

protected: //commented while Document still uses these

        // get/convert to the underlying model pointers
    Site *getSite() const { return _site; }

public:
// can be protected once we no longer use it in Document
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

private:
    Site* _site;
};

#endif
